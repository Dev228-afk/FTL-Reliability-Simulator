#pragma once
// Simulator configuration. Loaded from a YAML subset (one nesting level) or JSON.
// Unknown keys are rejected so a typo can never silently fall back to a default.

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

#include "ftl/types.hpp"

namespace ftl {

struct Geometry {
  std::uint32_t channels = 1;
  std::uint32_t dies_per_channel = 1;
  std::uint32_t blocks_per_die = 128;
  std::uint32_t pages_per_block = 256;
  std::uint32_t page_size_bytes = 4096;
  // Industry convention: OP% = (physical - user) / user * 100.
  double overprovision_percent = 20.0;

  std::uint32_t total_blocks() const { return channels * dies_per_channel * blocks_per_die; }
  std::uint64_t total_pages() const {
    return static_cast<std::uint64_t>(total_blocks()) * pages_per_block;
  }
};

// Synthetic NAND timing. These numbers are NOT sourced from any specific NAND
// generation; they only need to be in a plausible ratio (read << program << erase).
struct Timing {
  std::uint32_t read_us = 50;
  std::uint32_t program_us = 600;
  std::uint32_t erase_us = 3500;
  std::uint32_t jitter_us = 0;                 // uniform [0, jitter] added per op
  double wear_penalty_us_per_kcycle = 0.0;     // program/erase slowdown per 1000 P/E cycles
};

struct GcConfig {
  // Opportunistic GC keeps at least this many free blocks when possible.
  // One block is always reserved for GC destinations (see Ftl::kGcReserveBlocks).
  std::uint32_t low_watermark_blocks = 3;
  GcPolicy policy = GcPolicy::Greedy;
};

struct WearConfig {
  bool dynamic_enabled = true;          // allocate the least-worn free block
  bool static_enabled = true;           // relocate cold data off low-wear blocks
  std::uint32_t threshold = 8;          // max-min erase spread that triggers static WL
  std::uint32_t check_interval_erases = 16;
};

struct PersistenceConfig {
  RecoveryMode mode = RecoveryMode::Wal;
  std::uint32_t checkpoint_interval_ops = 1000;  // 0 disables periodic checkpoints
  bool checkpoint_on_flush = true;
};

struct DebugConfig {
  bool check_every_op = false;          // run structural invariants after every host op
  bool check_after_gc = true;
  bool check_after_checkpoint = true;
};

struct SimConfig {
  Geometry geometry;
  Timing timing;
  GcConfig gc;
  WearConfig wear;
  PersistenceConfig persistence;
  DebugConfig debug;
  std::uint64_t seed = 1;
  // Minimum spare blocks regardless of nominal OP. GC forward progress is only
  // guaranteed when user capacity <= (total_blocks - 4) * pages_per_block
  // (derivation in README "Capacity bound"). So "0% OP" still keeps 4 blocks.
  std::uint32_t min_spare_blocks = 4;

  std::uint64_t logical_pages() const;
  double effective_op_percent() const;
  void validate() const;  // throws std::invalid_argument
};

nlohmann::json config_to_json(const SimConfig& cfg);
SimConfig config_from_json(const nlohmann::json& j);   // throws on unknown keys / bad types

// Parses the YAML subset used in configs/*.yaml into JSON:
//   section:
//     key: value     # comment
//   top_key: value
nlohmann::json parse_simple_yaml(const std::string& text);

// Loads .json or .yaml/.yml by extension. Validates.
SimConfig load_config_file(const std::string& path);

}  // namespace ftl
