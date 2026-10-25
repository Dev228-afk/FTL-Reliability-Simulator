#include "workload/report.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace ftl {

nlohmann::json SimulationReport::to_json() const {
  nlohmann::json j;
  j["run_id"] = run_id;
  j["seed"] = seed;
  j["recovery_mode"] = recovery_mode;

  j["trace_stats"]["total_ops"] = total_trace_ops;
  j["trace_stats"]["host_writes"] = host_writes;
  j["trace_stats"]["host_reads"] = host_reads;
  j["trace_stats"]["host_trims"] = host_trims;

  j["gc_stats"]["gc_cycles"] = gc_cycles;
  j["gc_stats"]["gc_pages_relocated"] = gc_pages_relocated;
  j["gc_stats"]["write_amplification"] = write_amplification;

  j["wear"]["min_erase"] = wear.min_erase;
  j["wear"]["max_erase"] = wear.max_erase;
  j["wear"]["mean_erase"] = wear.mean_erase;
  j["wear"]["stddev_erase"] = wear.stddev_erase;

  j["nand"]["reads"] = nand_stats.total_reads;
  j["nand"]["programs"] = nand_stats.total_programs;
  j["nand"]["erases"] = nand_stats.total_erases;
  j["nand"]["simulated_time_us"] = nand_stats.simulated_time_us;

  j["crash"]["crashed"] = crashed;
  j["crash"]["point"] = crash_point;
  j["crash"]["seq"] = crash_seq;

  j["recovery"]["attempted"] = recovery_attempted;
  j["recovery"]["success"] = recovery_success;

  j["invariants"]["structural_passed"] = structural_invariants_passed;
  j["invariants"]["durability_passed"] = durability_invariants_passed;

  nlohmann::json viols = nlohmann::json::array();
  for (const auto& v : violations) {
    viols.push_back({{"name", v.name}, {"details", v.details}});
  }
  j["invariants"]["violations"] = viols;

  return j;
}

void SimulationReport::save_json(const std::string& path) const {
  std::ofstream fout(path);
  if (fout.is_open()) {
    fout << to_json().dump(2) << "\n";
  }
}

std::string SimulationReport::to_summary_string() const {
  std::ostringstream ss;
  ss << "=== FTL Simulation Report: " << run_id << " ===\n"
     << "Mode: " << recovery_mode << " | Seed: " << seed << "\n"
     << "Host Ops: " << total_trace_ops << " (W:" << host_writes << " R:" << host_reads << " T:" << host_trims << ")\n"
     << "GC Cycles: " << gc_cycles << " | Relocated: " << gc_pages_relocated << " | WAF: " << std::fixed << std::setprecision(2) << write_amplification << "\n"
     << "Erase Counts: [min: " << wear.min_erase << ", max: " << wear.max_erase << ", mean: " << wear.mean_erase << ", stddev: " << wear.stddev_erase << "]\n"
     << "Crashed: " << (crashed ? ("YES (" + crash_point + " at seq " + std::to_string(crash_seq) + ")") : "NO") << "\n"
     << "Recovery: " << (recovery_attempted ? (recovery_success ? "PASSED" : "FAILED") : "N/A") << "\n"
     << "Invariants: " << ((structural_invariants_passed && durability_invariants_passed) ? "ALL PASS" : "FAIL") << "\n";
  if (!violations.empty()) {
    ss << "Violations (" << violations.size() << "):\n";
    for (const auto& v : violations) {
      ss << "  - [" << v.name << "] " << v.details << "\n";
    }
  }
  return ss.str();
}

}  // namespace ftl
