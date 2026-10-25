#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "ftl/config.hpp"
#include "ftl/rng.hpp"
#include "recovery/crash.hpp"
#include "workload/report.hpp"
#include "workload/runner.hpp"
#include "workload/trace.hpp"

namespace {

void print_help() {
  std::cout << "FTL Reliability Simulator CLI (ftl-sim)\n"
            << "Usage:\n"
            << "  ftl-sim run --config <config.yaml/json> --trace <trace.jsonl> [options]\n"
            << "    Options:\n"
            << "      --work-dir <dir>          Directory for persistence/WAL (default: ./ftl_run)\n"
            << "      --crash-point <point>     Crash point (e.g. AFTER_ACK, BEFORE_MAPPING_UPDATE)\n"
            << "      --crash-seq <seq>         Sequence number on which to trigger crash\n"
            << "      --output <report.json>    Path to write structured JSON report\n\n"
            << "  ftl-sim generate-trace [options]\n"
            << "    Options:\n"
            << "      --type <seq_write|rand_write|mixed_70_30|churn>  Workload profile (default: mixed_70_30)\n"
            << "      --ops <count>             Number of operations (default: 5000)\n"
            << "      --lbas <count>            Logical capacity in pages (default: 1024)\n"
            << "      --seed <num>              RNG seed (default: 42)\n"
            << "      --output <trace.jsonl>    Output file path\n\n"
            << "  ftl-sim --help                Show this message\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    print_help();
    return 1;
  }

  std::string command = argv[1];
  if (command == "--help" || command == "-h") {
    print_help();
    return 0;
  }

  if (command == "generate-trace") {
    std::string type = "mixed_70_30";
    std::uint32_t ops = 5000;
    std::uint32_t lba_count = 1024;
    std::uint64_t seed = 42;
    std::string out_path = "trace.jsonl";

    for (int i = 2; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--type" && i + 1 < argc) type = argv[++i];
      else if (arg == "--ops" && i + 1 < argc) ops = std::stoul(argv[++i]);
      else if (arg == "--lbas" && i + 1 < argc) lba_count = std::stoul(argv[++i]);
      else if (arg == "--seed" && i + 1 < argc) seed = std::stoull(argv[++i]);
      else if (arg == "--output" && i + 1 < argc) out_path = argv[++i];
    }

    ftl::Rng rng(seed);
    std::vector<ftl::TraceRecord> records;
    records.reserve(ops);

    for (std::uint32_t s = 1; s <= ops; ++s) {
      ftl::TraceRecord rec;
      rec.seq = s;

      if (type == "seq_write") {
        rec.op = ftl::TraceOp::Write;
        rec.lba = (s - 1) % lba_count;
      } else if (type == "rand_write") {
        rec.op = ftl::TraceOp::Write;
        rec.lba = static_cast<ftl::Lba>(rng.uniform(lba_count));
      } else if (type == "churn") {
        // High overwrite on small hot set
        rec.op = ftl::TraceOp::Write;
        rec.lba = static_cast<ftl::Lba>(rng.uniform(std::max(10U, lba_count / 8)));
      } else {
        // mixed_70_30 (70% writes, 25% reads, 5% trims)
        double r = rng.unit();
        if (r < 0.70) rec.op = ftl::TraceOp::Write;
        else if (r < 0.95) rec.op = ftl::TraceOp::Read;
        else rec.op = ftl::TraceOp::Trim;
        rec.lba = static_cast<ftl::Lba>(rng.uniform(lba_count));
      }

      if (rec.op == ftl::TraceOp::Write) {
        rec.data.resize(4096);
        std::uint8_t pat = static_cast<std::uint8_t>((rec.lba + s) & 0xFF);
        std::fill(rec.data.begin(), rec.data.end(), pat);
      }
      records.push_back(rec);
    }

    ftl::TraceParser::save_to_file(out_path, records);
    std::cout << "Generated " << records.size() << " trace records into " << out_path << "\n";
    return 0;
  }

  if (command == "run") {
    std::string config_path;
    std::string trace_path;
    std::string work_dir = "./ftl_run";
    std::string crash_pt_str = "NONE";
    std::uint32_t crash_seq = 0;
    std::string output_path;

    for (int i = 2; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--config" && i + 1 < argc) config_path = argv[++i];
      else if (arg == "--trace" && i + 1 < argc) trace_path = argv[++i];
      else if (arg == "--work-dir" && i + 1 < argc) work_dir = argv[++i];
      else if (arg == "--crash-point" && i + 1 < argc) crash_pt_str = argv[++i];
      else if (arg == "--crash-seq" && i + 1 < argc) crash_seq = std::stoul(argv[++i]);
      else if (arg == "--output" && i + 1 < argc) output_path = argv[++i];
    }

    if (config_path.empty() || trace_path.empty()) {
      std::cerr << "Error: --config and --trace are required for 'run'\n";
      return 1;
    }

    try {
      auto config = ftl::load_config_file(config_path);
      ftl::WorkloadRunner runner(config, work_dir);

      ftl::CrashPoint cp = ftl::CrashPoint::None;
      if (ftl::parse_crash_point(crash_pt_str, cp) && cp != ftl::CrashPoint::None) {
        runner.set_crash_trigger({true, crash_seq, cp});
      }

      auto report = runner.run_trace(trace_path);
      std::cout << report.to_summary_string() << "\n";

      if (!output_path.empty()) {
        report.save_json(output_path);
        std::cout << "Report written to " << output_path << "\n";
      }

      bool pass = report.structural_invariants_passed && report.durability_invariants_passed;
      return pass ? 0 : 2;
    } catch (const std::exception& e) {
      std::cerr << "Execution failed: " << e.what() << "\n";
      return 1;
    }
  }

  std::cerr << "Unknown command: " << command << "\n";
  print_help();
  return 1;
}
