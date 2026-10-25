#include <gtest/gtest.h>

#include <fstream>

#include "recovery/wal.hpp"
#include "test_helpers.hpp"

namespace ftl {

TEST(WalTest, AppendAndReadRecords) {
  testing::TempDir temp_dir;
  std::string wal_path = temp_dir.path() + "/wal.log";

  WalManager wal;
  wal.open(wal_path);

  wal.append_write(1, 10, 100, 0x1111);
  wal.append_trim(2, 20);
  wal.append_checkpoint_marker(3);

  auto records = wal.read_records();
  ASSERT_EQ(records.size(), 3U);

  EXPECT_EQ(records[0].op, static_cast<std::uint8_t>(WalOp::Write));
  EXPECT_EQ(records[0].seq, 1U);
  EXPECT_EQ(records[0].lba, 10U);
  EXPECT_EQ(records[0].ppa, 100U);
  EXPECT_EQ(records[0].data_crc, 0x1111U);

  EXPECT_EQ(records[1].op, static_cast<std::uint8_t>(WalOp::Trim));
  EXPECT_EQ(records[1].seq, 2U);
  EXPECT_EQ(records[1].lba, 20U);

  EXPECT_EQ(records[2].op, static_cast<std::uint8_t>(WalOp::CheckpointMarker));
  EXPECT_EQ(records[2].seq, 3U);
}

TEST(WalTest, TornWriteDetection) {
  testing::TempDir temp_dir;
  std::string wal_path = temp_dir.path() + "/wal.log";

  WalManager wal;
  wal.open(wal_path);

  wal.append_write(1, 10, 100, 0x1111);
  wal.append_write(2, 20, 200, 0x2222);

  // Corrupt the end of the file by appending partial bytes (simulating sudden power loss during write)
  {
    std::ofstream fout(wal_path, std::ios::binary | std::ios::app);
    char partial[] = {0x57, 0x41, 0x4C, 0x31, 0x01};  // Magic + op but truncated
    fout.write(partial, sizeof(partial));
  }

  auto records = wal.read_records();
  // Torn write must be detected and discarded, keeping the 2 complete valid records
  EXPECT_EQ(records.size(), 2U);
}

}  // namespace ftl
