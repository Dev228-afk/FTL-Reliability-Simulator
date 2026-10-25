#pragma once
// Detailed NAND Flash Model.
// Implements physics-level constraints of NAND flash:
// 1. Pages must be programmed sequentially within a block (or at minimum write-once after erase).
// 2. Erase-before-write: Overwriting a programmed page without an erase is rejected.
// 3. Block-level erase granularity.
// 4. Configurable read/program/erase latencies, jitter, and endurance wear penalties.
// 5. Explicit physical metadata / OOB per page (LBA, sequence, CRC).

#include <cstdint>
#include <string>
#include <vector>

#include "ftl/config.hpp"
#include "ftl/rng.hpp"
#include "ftl/types.hpp"

namespace ftl {

struct PageMetadata {
  Lba lba = kInvalidLba;
  std::uint32_t seq = 0;
  std::uint32_t crc = 0;
};

struct BlockStats {
  std::uint32_t erase_count = 0;
  std::uint32_t programmed_pages = 0;
};

struct WearDistribution {
  std::uint32_t min_erase = 0;
  std::uint32_t max_erase = 0;
  double mean_erase = 0.0;
  double stddev_erase = 0.0;
};

struct NandStats {
  std::uint64_t total_reads = 0;
  std::uint64_t total_programs = 0;
  std::uint64_t total_erases = 0;
  std::uint64_t simulated_time_us = 0;
};

class NandModel {
 public:
  explicit NandModel(const SimConfig& config);

  const Geometry& geometry() const { return config_.geometry; }
  const SimConfig& config() const { return config_; }

  Ppa decode_ppa(PpaIndex ppa) const;
  PpaIndex encode_ppa(const Ppa& decoded) const;
  std::uint32_t block_of_ppa(PpaIndex ppa) const;
  std::uint32_t page_in_block(PpaIndex ppa) const;

  // Raw NAND Operations
  bool is_page_programmed(PpaIndex ppa) const;
  void program_page(PpaIndex ppa, const std::vector<std::uint8_t>& data, const PageMetadata& meta);
  void read_page(PpaIndex ppa, std::vector<std::uint8_t>& out_data, PageMetadata& out_meta);
  void erase_block(std::uint32_t block_index);

  // Persistence snapshot of raw NAND storage
  void dump_raw(std::vector<std::uint8_t>& out_bytes) const;
  void load_raw(const std::vector<std::uint8_t>& in_bytes);
  bool save_to_file(const std::string& path) const;
  bool load_from_file(const std::string& path);

  // Erase counts and wear inspection
  std::uint32_t get_erase_count(std::uint32_t block_index) const;
  void set_erase_count(std::uint32_t block_index, std::uint32_t count);
  WearDistribution calculate_wear_distribution() const;

  const NandStats& stats() const { return stats_; }
  void reset_stats();

 private:
  SimConfig config_;
  Rng rng_;

  std::vector<bool> programmed_;
  std::vector<std::vector<std::uint8_t>> storage_;
  std::vector<PageMetadata> metadata_;
  std::vector<BlockStats> blocks_;

  NandStats stats_;

  void account_latency(std::uint32_t base_us, double wear_cycles);
};

}  // namespace ftl
