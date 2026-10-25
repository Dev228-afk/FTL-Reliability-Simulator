#include <gtest/gtest.h>

#include "ftl/ftl.hpp"
#include "invariants/invariants.hpp"
#include "test_helpers.hpp"

namespace ftl {

TEST(RestartTest, CleanCheckpointRestartRestoresAllMappings) {
  testing::TempDir temp_dir;
  SimConfig cfg = testing::make_test_config(RecoveryMode::Wal, 8, 16);

  std::map<Lba, std::pair<std::uint32_t, std::vector<std::uint8_t>>> written;

  {
    Ftl ftl(cfg, temp_dir.path());
    for (std::uint32_t i = 1; i <= 40; ++i) {
      Lba lba = i % 15;
      std::vector<std::uint8_t> data(4096, static_cast<std::uint8_t>(i));
      ftl.write(lba, data, i);
      written[lba] = {i, data};
    }
    ftl.checkpoint();
  }

  // Create new FTL instance representing clean restart
  {
    Ftl ftl2(cfg, temp_dir.path());
    EXPECT_TRUE(ftl2.recover());

    auto s_rep = InvariantChecker::check_structural(ftl2);
    EXPECT_TRUE(s_rep.passed);

    auto d_rep = InvariantChecker::check_acknowledged_writes(ftl2, written);
    EXPECT_TRUE(d_rep.passed);
  }
}

}  // namespace ftl
