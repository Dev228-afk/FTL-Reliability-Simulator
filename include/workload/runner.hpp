#pragma once
// Workload Execution & Crash-Recovery Runner.
// Executes workload traces against an FTL instance, orchestrates crash injection,
// verifies crash recovery, and validates storage-system invariants.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "ftl/config.hpp"
#include "ftl/ftl.hpp"
#include "workload/report.hpp"
#include "workload/trace.hpp"

namespace ftl {

class WorkloadRunner {
 public:
  WorkloadRunner(const SimConfig& config, const std::string& work_dir);

  // Set crash trigger parameters
  void set_crash_trigger(const CrashTrigger& trigger);

  // Execute trace from file
  SimulationReport run_trace(const std::string& trace_path);

  // Execute in-memory trace records
  SimulationReport run_records(const std::vector<TraceRecord>& records);

 private:
  SimConfig config_;
  std::string work_dir_;
  CrashTrigger crash_trigger_;
};

}  // namespace ftl
