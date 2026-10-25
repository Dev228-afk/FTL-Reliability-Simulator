#include <gtest/gtest.h>

#include "ftl/checksum.hpp"

namespace ftl {

TEST(ChecksumTest, Crc32Empty) {
  EXPECT_EQ(crc32("", 0), 0x00000000U);
}

TEST(ChecksumTest, Crc32StandardVectors) {
  // "123456789" standard check value for IEEE 802.3 CRC32 is 0xCBF43926
  std::string input = "123456789";
  std::uint32_t val = crc32(input);
  EXPECT_EQ(val, 0xCBF43926U);
  EXPECT_EQ(crc32_hex(val), "cbf43926");

  std::uint32_t parsed = 0;
  EXPECT_TRUE(parse_crc32_hex("cbf43926", parsed));
  EXPECT_EQ(parsed, 0xCBF43926U);
}

TEST(ChecksumTest, Crc32SeedChaining) {
  std::string part1 = "1234";
  std::string part2 = "56789";

  std::uint32_t full = crc32("123456789");
  std::uint32_t c1 = crc32(part1);
  std::uint32_t c2 = crc32(part2.data(), part2.size(), c1);
  EXPECT_EQ(c2, full);
}

}  // namespace ftl
