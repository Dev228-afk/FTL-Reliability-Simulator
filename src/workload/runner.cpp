#include "workload/runner.hpp"

#include <iostream>
#include <map>
#include <memory>

#include "invariants/invariants.hpp"

namespace ftl {

WorkloadRunner::WorkloadRunner(const SimConfig& config, const std::string& work_dir)
    : config_(config), work_dir_(work_dir) {}

void WorkloadRunner::set_crash_trigger(const CrashTrigger& trigger) {
  crash_trigger_ = trigger;
}

SimulationReport WorkloadRunner::run_trace(const std::string& trace_path) {
  auto records = TraceParser::load_from_file(trace_path);
  return run_records(records);
}

SimulationReport WorkloadRunner::run_records(const std::vector<TraceRecord>& records) {
  SimulationReport rep;
  rep.run_id = "run_" + std::to_string(config_.seed);
  rep.seed = config_.seed;
  rep.recovery_mode = to_string(config_.persistence.mode);
  rep.total_trace_ops = records.size();

  std::map<Lba, std::pair<std::uint32_t, std::vector<std::uint8_t>>> acked_writes;

  auto ftl = std::make_unique<Ftl>(config_, work_dir_);
  ftl->crash_injector().set_trigger(crash_trigger_);

  try {
    for (const auto& rec : records) {
      if (rec.op == TraceOp::Write) {
        ftl->write(rec.lba, rec.data, rec.seq);
        acked_writes[rec.lba] = {rec.seq, rec.data};
      } else if (rec.op == TraceOp::Read) {
        std::vector<std::uint8_t> read_buf;
        ftl->read(rec.lba, read_buf, rec.seq);
      } else if (rec.op == TraceOp::Trim) {
        ftl->trim(rec.lba, rec.seq);
        acked_writes.erase(rec.lba);
      } else if (rec.op == TraceOp::Flush) {
        ftl->flush(rec.seq);
      }
    }
  } catch (const CrashException& ex) {
    rep.crashed = true;
    rep.crash_point = to_string(ex.point());
    rep.crash_seq = ex.seq();
    if (ex.point() == CrashPoint::AfterAck) {
      for (const auto& rec : records) {
        if (rec.seq == ex.seq()) {
          if (rec.op == TraceOp::Write) {
            acked_writes[rec.lba] = {rec.seq, rec.data};
          } else if (rec.op == TraceOp::Trim) {
            acked_writes.erase(rec.lba);
          }
          break;
        }
      }
    }
  }

  // Record baseline operational metrics
  const auto& fstats = ftl->stats();
  rep.host_writes = fstats.host_writes;
  rep.host_reads = fstats.host_reads;
  rep.host_trims = fstats.host_trims;
  rep.gc_cycles = fstats.gc_cycles;
  rep.gc_pages_relocated = fstats.gc_pages_relocated;
  rep.write_amplification = fstats.write_amplification();

  rep.wear = ftl->nand().calculate_wear_distribution();
  rep.nand_stats = ftl->nand().stats();

  if (rep.crashed) {
    // Abrupt termination simulation: destroy old instance and create a fresh one
    ftl.reset();
    rep.recovery_attempted = true;

    auto recovered_ftl = std::make_unique<Ftl>(config_, work_dir_);
    bool rec_ok = recovered_ftl->recover();
    rep.recovery_success = rec_ok;

    // Check structural invariants on recovered FTL
    auto struct_rep = InvariantChecker::check_structural(*recovered_ftl);
    rep.structural_invariants_passed = struct_rep.passed;
    for (const auto& v : struct_rep.violations) {
      rep.violations.push_back(v);
    }

    // Check durability invariants on host-acknowledged writes
    // Note: in vulnerable mode, uncheckpointed acknowledged writes fail here!
    auto dur_rep = InvariantChecker::check_acknowledged_writes(*recovered_ftl, acked_writes);
    rep.durability_invariants_passed = dur_rep.passed;
    for (const auto& v : dur_rep.violations) {
      rep.violations.push_back(v);
    }
  } else {
    // Normal completion: verify invariants directly
    auto struct_rep = InvariantChecker::check_structural(*ftl);
    rep.structural_invariants_passed = struct_rep.passed;
    for (const auto& v : struct_rep.violations) {
      rep.violations.push_back(v);
    }

    auto dur_rep = InvariantChecker::check_acknowledged_writes(*ftl, acked_writes);
    rep.durability_invariants_passed = dur_rep.passed;
    for (const auto& v : dur_rep.violations) {
      rep.violations.push_back(v);
    }
  }

  return rep;
}

}  // namespace ftl
