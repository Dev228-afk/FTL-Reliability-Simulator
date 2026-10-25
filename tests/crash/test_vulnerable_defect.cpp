#include <gtest/gtest.h>

#include "ftl/checksum.hpp"
#include "ftl/ftl.hpp"
#include "invariants/invariants.hpp"
#include "recovery/crash.hpp"
#include "test_helpers.hpp"
#include "workload/runner.hpp"

namespace ftl {

TEST(VulnerableDefectTest, VulnerableModeLosesAcknowledgedWritesAfterCrash) {
  testing::TempDir temp_dir;
  // Vulnerable mode: checkpoint only, no WAL
  SimConfig cfg = testing::make_test_config(RecoveryMode::Vulnerable, 8, 16);
  cfg.persistence.checkpoint_interval_ops = 1000;  // High interval so checkpoint hasn't occurred

  std::vector<TraceRecord> trace;
  for (std::uint32_t i = 1; i <= 20; ++i) {
    TraceRecord r;
    r.seq = i;
    r.op = TraceOp::Write;
    r.lba = i % 10;
    r.data.assign(4096, static_cast<std::uint8_t>(i));
    r.crc = crc32(r.data.data(), r.data.size());
    trace.push_back(r);
  }

  WorkloadRunner runner(cfg, temp_dir.path());
  // Inject crash at seq 15 AFTER_ACK (host has received acknowledgement!)
  runner.set_crash_trigger({true, 15, CrashPoint::AfterAck});

  SimulationReport rep = runner.run_records(trace);

  EXPECT_TRUE(rep.crashed);
  EXPECT_TRUE(rep.recovery_attempted);
  // In vulnerable mode, uncheckpointed writes are lost, so durability invariants MUST detect failure!
  EXPECT_FALSE(rep.durability_invariants_passed);
  EXPECT_FALSE(rep.violations.empty());
}

TEST(VulnerableDefectTest, WalModeSurvivesIdenticalCrashWithZeroViolations) {
  testing::TempDir temp_dir;
  // Fixed WAL mode
  SimConfig cfg = testing::make_test_config(RecoveryMode::Wal, 8, 16);
  cfg.persistence.checkpoint_interval_ops = 1000;

  std::vector<TraceRecord> trace;
  for (std::uint32_t i = 1; i <= 20; ++i) {
    TraceRecord r;
    r.seq = i;
    r.op = TraceOp::Write;
    r.lba = i % 10;
    r.data.assign(4096, static_cast<std::uint8_t>(i));
    r.crc = crc32(r.data.data(), r.data.size());
    trace.push_back(r);
  }

  WorkloadRunner runner(cfg, temp_dir.path());
  // Inject identical crash at seq 15 AFTER_ACK
  runner.set_crash_trigger({true, 15, CrashPoint::AfterAck});

  SimulationReport rep = runner.run_records(trace);

  EXPECT_TRUE(rep.crashed);
  EXPECT_TRUE(rep.recovery_attempted);
  EXPECT_TRUE(rep.recovery_success);

  // In WAL mode, WAL replay restores all acknowledged writes! Both structural & durability invariants pass!
  EXPECT_TRUE(rep.structural_invariants_passed);
  EXPECT_TRUE(rep.durability_invariants_passed);
  EXPECT_TRUE(rep.violations.empty());
}

}  // namespace ftl
