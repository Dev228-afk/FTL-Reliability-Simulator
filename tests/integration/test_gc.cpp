#include <gtest/gtest.h>

#include "ftl/ftl.hpp"
#include "invariants/invariants.hpp"
#include "test_helpers.hpp"

namespace ftl {

TEST(GcIntegrationTest, OutOfPlaceWritesTriggerGc) {
  testing::TempDir temp_dir;
  // 8 blocks, 16 pages per block = 128 total physical pages
  SimConfig cfg = testing::make_test_config(RecoveryMode::Wal, 8, 16);
  cfg.gc.low_watermark_blocks = 2;
  Ftl ftl(cfg, temp_dir.path());

  std::vector<std::uint8_t> data(4096, 0x77);

  // Populate 4 full blocks (64 pages) of data
  for (std::uint32_t i = 0; i < 64; ++i) {
    ftl.write(i, data, i + 1);
  }

  // Repeatedly overwrite only a subset of LBAs (0..7) so victim blocks contain
  // both invalid pages and preserved valid pages (8..15) that must be relocated.
  for (std::uint32_t i = 0; i < 120; ++i) {
    Lba target = (i % 8);
    ftl.write(target, data, 1000 + i);
  }

  // GC should have run and relocated valid pages
  EXPECT_GT(ftl.stats().gc_cycles, 0U);
  EXPECT_GT(ftl.stats().gc_pages_relocated, 0U);
  EXPECT_GT(ftl.stats().write_amplification(), 1.0);

  // Invariants must hold after extensive GC
  auto rep = InvariantChecker::check_structural(ftl);
  EXPECT_TRUE(rep.passed);
  if (!rep.passed) {
    for (const auto& v : rep.violations) {
      std::cout << "Violation: " << v.name << " - " << v.details << "\n";
    }
  }
}

}  // namespace ftl
