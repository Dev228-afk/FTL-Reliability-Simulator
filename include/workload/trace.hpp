#pragma once
// Workload Trace Engine.
// Reads and writes deterministic trace records in JSONL format.
// Compatible with both Python trace generators and C++ workload runner.

#include <cstdint>
#include <string>
#include <vector>

#include "ftl/types.hpp"

namespace ftl {

enum class TraceOp : std::uint8_t {
  Write = 1,
  Read = 2,
  Trim = 3,
  Flush = 4
};

const char* to_string(TraceOp op);
bool parse_trace_op(const std::string& text, TraceOp& out);

struct TraceRecord {
  std::uint32_t seq = 0;
  TraceOp op = TraceOp::Write;
  Lba lba = kInvalidLba;
  std::vector<std::uint8_t> data;
  std::uint32_t crc = 0;
};

class TraceParser {
 public:
  static std::vector<TraceRecord> load_from_file(const std::string& path);
  static void save_to_file(const std::string& path, const std::vector<TraceRecord>& records);
};

}  // namespace ftl
