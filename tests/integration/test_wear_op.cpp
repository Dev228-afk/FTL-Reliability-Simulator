#include <gtest/gtest.h>

#include "ftl/ftl.hpp"
#include "invariants/invariants.hpp"
#include "test_helpers.hpp"

namespace ftl {

TEST(WearLevelingTest, WearDistributionRemainsEven) {
  testing::TempDir temp_dir;
  SimConfig cfg = testing::make_test_config(RecoveryMode::Wal, 8, 16);
  cfg.wear.dynamic_enabled = true;
  cfg.wear.static_enabled = true;
  cfg.wear.threshold = 4;
  Ftl ftl(cfg, temp_dir.path());

  std::vector<std::uint8_t> data(4096, 0x55);

  // Issue writes
  for (std::uint32_t i = 1; i <= 250; ++i) {
    ftl.write(i % 30, data, i);
  }

  auto dist = ftl.nand().calculate_wear_distribution();
  EXPECT_GT(dist.max_erase, 0U);
  // Spread should not explode thanks to wear leveling
  EXPECT_LE(dist.max_erase - dist.min_erase, 10U);

  auto rep = InvariantChecker::check_structural(ftl);
  EXPECT_TRUE(rep.passed);
}

}  // namespace ftl
