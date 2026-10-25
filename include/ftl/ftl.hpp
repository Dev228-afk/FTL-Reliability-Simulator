#pragma once
// Core Flash Translation Layer (FTL).
// Implements:
// - Page-level Logical-to-Physical (L2P) and reverse (P2L) mapping.
// - Wear-aware sequential free-page allocator.
// - Garbage collection (Greedy & Cost-Benefit victim selection).
// - Static and dynamic wear leveling.
// - Dual-mode durability:
//     * Vulnerable mode (checkpoint-only, reproducing the resume defect).
//     * WAL mode (write-ahead logged, proving recovery invariance).
// - Crash injection hooks at all critical ordering boundaries.

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ftl/config.hpp"
#include "ftl/rng.hpp"
#include "ftl/types.hpp"
#include "nand/nand.hpp"
#include "recovery/checkpoint.hpp"
#include "recovery/crash.hpp"
#include "recovery/wal.hpp"

namespace ftl {

struct FtlStats {
  std::uint64_t host_writes = 0;
  std::uint64_t host_reads = 0;
  std::uint64_t host_trims = 0;
  std::uint64_t gc_cycles = 0;
  std::uint64_t gc_pages_relocated = 0;
  std::uint64_t static_wear_level_events = 0;

  double write_amplification() const;
};

class Ftl {
 public:
  explicit Ftl(const SimConfig& config, const std::string& work_dir = ".");

  // Accessors
  const SimConfig& config() const { return config_; }
  const NandModel& nand() const { return nand_; }
  NandModel& nand() { return nand_; }
  const FtlStats& stats() const { return stats_; }
  CrashInjector& crash_injector() { return crash_; }
  const CrashInjector& crash_injector() const { return crash_; }

  // Internal table inspection for invariant verification
  const std::vector<PpaIndex>& l2p() const { return l2p_; }
  const std::vector<Lba>& p2l() const { return p2l_; }
  const std::vector<PageState>& page_states() const { return page_states_; }
  const std::vector<BlockState>& block_states() const { return block_states_; }
  std::uint32_t valid_pages(std::uint32_t block) const { return valid_pages_[block]; }
  std::uint32_t invalid_pages(std::uint32_t block) const { return invalid_pages_[block]; }
  std::uint32_t free_pages(std::uint32_t block) const { return free_pages_[block]; }

  // Host Storage Operations
  void write(Lba lba, const std::vector<std::uint8_t>& data, std::uint32_t seq);
  bool read(Lba lba, std::vector<std::uint8_t>& out_data, std::uint32_t seq);
  void trim(Lba lba, std::uint32_t seq);
  void flush(std::uint32_t seq);

  // Persistence & Recovery
  void checkpoint();
  bool recover();

  // Garbage Collection & Wear Leveling
  bool trigger_gc();
  void perform_static_wear_leveling();

  // Helper count of free blocks
  std::uint32_t count_free_blocks() const;

 private:
  SimConfig config_;
  std::string work_dir_;
  NandModel nand_;
  WalManager wal_;
  CrashInjector crash_;
  Rng rng_;
  FtlStats stats_;

  std::uint32_t checkpoint_seq_ = 0;
  std::uint32_t ops_since_checkpoint_ = 0;
  std::uint32_t erases_since_wear_check_ = 0;

  // Mapping structures
  std::vector<PpaIndex> l2p_;
  std::vector<Lba> p2l_;
  std::vector<PageState> page_states_;
  std::vector<BlockState> block_states_;

  std::vector<std::uint32_t> valid_pages_;
  std::vector<std::uint32_t> invalid_pages_;
  std::vector<std::uint32_t> free_pages_;

  // Allocator state
  std::uint32_t user_open_block_ = kInvalidPpa;
  std::uint32_t user_next_page_ = 0;
  std::uint32_t gc_open_block_ = kInvalidPpa;
  std::uint32_t gc_next_page_ = 0;

  // Internal allocation helpers
  PpaIndex allocate_user_page();
  PpaIndex allocate_gc_page();
  std::uint32_t pick_free_block(bool gc_block = false);
  std::uint32_t select_gc_victim();
  void close_block(std::uint32_t block_index);
};

}  // namespace ftl
