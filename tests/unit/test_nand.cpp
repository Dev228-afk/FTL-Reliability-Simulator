#include <gtest/gtest.h>

#include "nand/nand.hpp"
#include "test_helpers.hpp"

namespace ftl {

TEST(NandTest, EraseBeforeWriteConstraint) {
  SimConfig cfg = testing::make_test_config();
  NandModel nand(cfg);

  PpaIndex ppa = 0;
  std::vector<std::uint8_t> data(4096, 0xAA);
  PageMetadata meta{10, 1, 0x1234};

  EXPECT_FALSE(nand.is_page_programmed(ppa));
  nand.program_page(ppa, data, meta);
  EXPECT_TRUE(nand.is_page_programmed(ppa));

  // Overwriting without erase must throw
  EXPECT_THROW(nand.program_page(ppa, data, meta), std::runtime_error);

  // Erase the block
  std::uint32_t b = nand.block_of_ppa(ppa);
  nand.erase_block(b);
  EXPECT_FALSE(nand.is_page_programmed(ppa));

  // Now program is allowed again
  EXPECT_NO_THROW(nand.program_page(ppa, data, meta));
}

TEST(NandTest, ReadProgramDataIntegrity) {
  SimConfig cfg = testing::make_test_config();
  NandModel nand(cfg);

  PpaIndex ppa = 5;
  std::vector<std::uint8_t> data(4096, 0x42);
  PageMetadata meta{100, 42, 0xCAFEBABE};

  nand.program_page(ppa, data, meta);

  std::vector<std::uint8_t> out_data;
  PageMetadata out_meta;
  nand.read_page(ppa, out_data, out_meta);

  EXPECT_EQ(out_data, data);
  EXPECT_EQ(out_meta.lba, 100U);
  EXPECT_EQ(out_meta.seq, 42U);
  EXPECT_EQ(out_meta.crc, 0xCAFEBABEU);
}

TEST(NandTest, WearDistribution) {
  SimConfig cfg = testing::make_test_config();
  NandModel nand(cfg);

  nand.erase_block(0);
  nand.erase_block(0);
  nand.erase_block(1);

  auto dist = nand.calculate_wear_distribution();
  EXPECT_EQ(dist.min_erase, 0U);
  EXPECT_EQ(dist.max_erase, 2U);
  EXPECT_GT(dist.mean_erase, 0.0);
}

}  // namespace ftl
