#pragma once
// Simulation Result Report.
// Encapsulates comprehensive statistics, endurance metrics, crash details,
// and invariant verification outcomes.

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "invariants/invariants.hpp"
#include "nand/nand.hpp"

namespace ftl {

struct SimulationReport {
  std::string run_id;
  std::uint64_t seed = 0;
  std::string recovery_mode;

  std::uint64_t total_trace_ops = 0;
  std::uint64_t host_writes = 0;
  std::uint64_t host_reads = 0;
  std::uint64_t host_trims = 0;

  std::uint64_t gc_cycles = 0;
  std::uint64_t gc_pages_relocated = 0;
  double write_amplification = 1.0;

  WearDistribution wear;
  NandStats nand_stats;

  bool crashed = false;
  std::string crash_point;
  std::uint32_t crash_seq = 0;

  bool recovery_attempted = false;
  bool recovery_success = false;

  bool structural_invariants_passed = true;
  bool durability_invariants_passed = true;
  std::vector<InvariantViolation> violations;

  nlohmann::json to_json() const;
  void save_json(const std::string& path) const;
  std::string to_summary_string() const;
};

}  // namespace ftl
