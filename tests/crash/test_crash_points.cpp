#include <gtest/gtest.h>

#include "ftl/ftl.hpp"
#include "recovery/crash.hpp"
#include "test_helpers.hpp"

namespace ftl {

TEST(CrashTest, InjectedCrashPointsThrowException) {
  testing::TempDir temp_dir;
  SimConfig cfg = testing::make_test_config(RecoveryMode::Wal, 8, 16);
  Ftl ftl(cfg, temp_dir.path());

  std::vector<std::uint8_t> data(4096, 0x12);

  // Set crash trigger at AfterDataProgram on seq 5
  ftl.crash_injector().set_trigger({true, 5, CrashPoint::AfterDataProgram});

  for (std::uint32_t s = 1; s < 5; ++s) {
    EXPECT_NO_THROW(ftl.write(s, data, s));
  }

  EXPECT_THROW(ftl.write(5, data, 5), CrashException);
}

}  // namespace ftl
