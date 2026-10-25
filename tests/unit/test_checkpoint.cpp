#include <gtest/gtest.h>

#include <fstream>

#include "recovery/checkpoint.hpp"
#include "test_helpers.hpp"

namespace ftl {

TEST(CheckpointTest, SaveAndLoadIntegrity) {
  testing::TempDir temp_dir;
  std::string cp_path = temp_dir.path() + "/checkpoint.meta";

  CheckpointData data;
  data.checkpoint_seq = 42;
  data.logical_pages = 100;
  data.physical_pages = 200;
  data.total_blocks = 10;
  data.user_open_block = 1;
  data.user_next_page = 5;
  data.gc_open_block = 2;
  data.gc_next_page = 0;

  data.l2p.assign(100, 50);
  data.p2l.assign(200, 25);
  data.page_states.assign(200, 1);
  data.erase_counts.assign(10, 3);
  data.block_states.assign(10, 3);

  EXPECT_TRUE(CheckpointManager::save(cp_path, data));

  CheckpointData restored;
  EXPECT_TRUE(CheckpointManager::load(cp_path, restored));

  EXPECT_EQ(restored.checkpoint_seq, 42U);
  EXPECT_EQ(restored.logical_pages, 100U);
  EXPECT_EQ(restored.physical_pages, 200U);
  EXPECT_EQ(restored.l2p, data.l2p);
  EXPECT_EQ(restored.p2l, data.p2l);
  EXPECT_EQ(restored.erase_counts, data.erase_counts);
}

TEST(CheckpointTest, CorruptedCheckpointRejection) {
  testing::TempDir temp_dir;
  std::string cp_path = temp_dir.path() + "/checkpoint.meta";

  CheckpointData data;
  data.checkpoint_seq = 1;
  data.logical_pages = 10;
  data.physical_pages = 20;
  data.total_blocks = 2;
  data.l2p.assign(10, 1);
  data.p2l.assign(20, 1);
  data.page_states.assign(20, 1);
  data.erase_counts.assign(2, 0);
  data.block_states.assign(2, 0);

  EXPECT_TRUE(CheckpointManager::save(cp_path, data));

  // Corrupt a byte in the payload
  {
    std::fstream f(cp_path, std::ios::in | std::ios::out | std::ios::binary);
    f.seekp(50);
    char bad = 0x77;
    f.write(&bad, 1);
  }

  CheckpointData bad_data;
  EXPECT_FALSE(CheckpointManager::load(cp_path, bad_data));
}

}  // namespace ftl
