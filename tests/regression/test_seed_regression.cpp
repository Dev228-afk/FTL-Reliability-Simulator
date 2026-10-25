#include <gtest/gtest.h>

#include "ftl/checksum.hpp"
#include "ftl/ftl.hpp"
#include "ftl/rng.hpp"
#include "invariants/invariants.hpp"
#include "recovery/crash.hpp"
#include "test_helpers.hpp"
#include "workload/runner.hpp"

namespace ftl {

// The 30 Fault Seeds Regression Test suite
// Proves the resume claim across 30 distinct random seeds and crash injection points:
// 1. In vulnerable mode, uncheckpointed ordering bugs reproducibly trigger invariant failures.
// 2. In WAL mode, write-ahead recovery reliably eliminates all corruption across all 30 seeds.
TEST(SeedRegressionTest, ThirtyFaultSeedsComparison) {
  constexpr std::uint32_t kSeedCount = 30;
  constexpr std::uint32_t kOpsPerTrace = 100;

  std::uint32_t vulnerable_failures = 0;
  std::uint32_t wal_failures = 0;

  for (std::uint32_t seed = 1; seed <= kSeedCount; ++seed) {
    Rng rng(seed * 7919);

    // Generate deterministic workload for this seed
    std::vector<TraceRecord> trace;
    trace.reserve(kOpsPerTrace);
    for (std::uint32_t s = 1; s <= kOpsPerTrace; ++s) {
      TraceRecord r;
      r.seq = s;
      double roll = rng.unit();
      if (roll < 0.70) {
        r.op = TraceOp::Write;
        r.lba = static_cast<Lba>(rng.uniform(40));
        r.data.assign(4096, static_cast<std::uint8_t>((seed + s) & 0xFF));
        r.crc = crc32(r.data.data(), r.data.size());
      } else if (roll < 0.90) {
        r.op = TraceOp::Read;
        r.lba = static_cast<Lba>(rng.uniform(40));
      } else {
        r.op = TraceOp::Trim;
        r.lba = static_cast<Lba>(rng.uniform(40));
      }
      trace.push_back(r);
    }

    // Pick deterministic crash point on an acknowledged write between ops 20 and 80
    std::vector<std::uint32_t> write_seqs;
    for (const auto& r : trace) {
      if (r.op == TraceOp::Write && r.seq >= 20 && r.seq <= 80) {
        write_seqs.push_back(r.seq);
      }
    }
    ASSERT_FALSE(write_seqs.empty());
    std::uint32_t crash_seq = write_seqs[rng.uniform(write_seqs.size())];
    CrashPoint crash_point = CrashPoint::AfterAck;

    // --- 1. Test Vulnerable Mode ---
    {
      testing::TempDir temp_dir;
      SimConfig cfg = testing::make_test_config(RecoveryMode::Vulnerable, 8, 16);
      cfg.seed = seed;
      cfg.persistence.checkpoint_interval_ops = 500;  // Will not checkpoint before crash

      WorkloadRunner runner(cfg, temp_dir.path());
      runner.set_crash_trigger({true, crash_seq, crash_point});
      SimulationReport rep = runner.run_records(trace);

      if (!rep.structural_invariants_passed || !rep.durability_invariants_passed) {
        vulnerable_failures++;
      }
    }

    // --- 2. Test Fixed WAL Mode ---
    {
      testing::TempDir temp_dir;
      SimConfig cfg = testing::make_test_config(RecoveryMode::Wal, 8, 16);
      cfg.seed = seed;
      cfg.persistence.checkpoint_interval_ops = 500;

      WorkloadRunner runner(cfg, temp_dir.path());
      runner.set_crash_trigger({true, crash_seq, crash_point});
      SimulationReport rep = runner.run_records(trace);

      if (!rep.structural_invariants_passed || !rep.durability_invariants_passed) {
        wal_failures++;
      }
    }
  }

  // Expect vulnerable mode to consistently fail recovery across all 30 seeds
  EXPECT_EQ(vulnerable_failures, kSeedCount);

  // Expect WAL mode to have ZERO observed corruption across all 30 seeds!
  EXPECT_EQ(wal_failures, 0U);
}

}  // namespace ftl
