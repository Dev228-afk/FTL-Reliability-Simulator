#pragma once
// FTL Checkpoint Persistence.
// Periodically snapshots the complete in-memory mapping and allocator tables.
// Protected by CRC32 to guarantee atomicity and detect partial writes on crash.

#include <cstdint>
#include <string>
#include <vector>

#include "ftl/types.hpp"

namespace ftl {

struct CheckpointData {
  std::uint32_t checkpoint_seq = 0;
  std::uint64_t logical_pages = 0;
  std::uint64_t physical_pages = 0;
  std::uint32_t total_blocks = 0;

  // L2P forward mapping
  std::vector<PpaIndex> l2p;
  // P2L reverse mapping
  std::vector<Lba> p2l;
  // Page states
  std::vector<std::uint8_t> page_states;
  // Block erase counts and states
  std::vector<std::uint32_t> erase_counts;
  std::vector<std::uint8_t> block_states;

  // Allocator state
  std::uint32_t user_open_block = 0;
  std::uint32_t user_next_page = 0;
  std::uint32_t gc_open_block = 0;
  std::uint32_t gc_next_page = 0;
};

class CheckpointManager {
 public:
  static bool save(const std::string& filepath, const CheckpointData& data);
  static bool load(const std::string& filepath, CheckpointData& out_data);
};

}  // namespace ftl
