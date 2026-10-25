#include <gtest/gtest.h>

#include "ftl/ftl.hpp"
#include "invariants/invariants.hpp"
#include "test_helpers.hpp"

namespace ftl {

TEST(MappingTest, BasicWriteReadTrim) {
  testing::TempDir temp_dir;
  SimConfig cfg = testing::make_test_config();
  Ftl ftl(cfg, temp_dir.path());

  Lba lba = 5;
  std::vector<std::uint8_t> data(4096, 0xEF);

  // Initial read of unmapped LBA should return zero
  std::vector<std::uint8_t> read_buf;
  EXPECT_TRUE(ftl.read(lba, read_buf, 1));
  EXPECT_EQ(read_buf, std::vector<std::uint8_t>(4096, 0));

  // Write
  ftl.write(lba, data, 2);

  // Read back
  EXPECT_TRUE(ftl.read(lba, read_buf, 3));
  EXPECT_EQ(read_buf, data);

  // Trim
  ftl.trim(lba, 4);

  // Post-trim read returns zero
  EXPECT_TRUE(ftl.read(lba, read_buf, 5));
  EXPECT_EQ(read_buf, std::vector<std::uint8_t>(4096, 0));

  // Invariant verification
  auto rep = InvariantChecker::check_structural(ftl);
  EXPECT_TRUE(rep.passed);
}

TEST(MappingTest, OverwriteInvalidatesOldPage) {
  testing::TempDir temp_dir;
  SimConfig cfg = testing::make_test_config();
  Ftl ftl(cfg, temp_dir.path());

  Lba lba = 10;
  std::vector<std::uint8_t> v1(4096, 0x11);
  std::vector<std::uint8_t> v2(4096, 0x22);

  ftl.write(lba, v1, 1);
  PpaIndex ppa1 = ftl.l2p()[lba];
  EXPECT_NE(ppa1, kInvalidPpa);
  EXPECT_EQ(ftl.page_states()[ppa1], PageState::Valid);

  // Overwrite
  ftl.write(lba, v2, 2);
  PpaIndex ppa2 = ftl.l2p()[lba];
  EXPECT_NE(ppa2, ppa1);
  EXPECT_EQ(ftl.page_states()[ppa1], PageState::Invalid);
  EXPECT_EQ(ftl.page_states()[ppa2], PageState::Valid);

  auto rep = InvariantChecker::check_structural(ftl);
  EXPECT_TRUE(rep.passed);
}

}  // namespace ftl
