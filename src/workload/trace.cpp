#include "workload/trace.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "ftl/checksum.hpp"

namespace ftl {

const char* to_string(TraceOp op) {
  switch (op) {
    case TraceOp::Write: return "WRITE";
    case TraceOp::Read: return "READ";
    case TraceOp::Trim: return "TRIM";
    case TraceOp::Flush: return "FLUSH";
  }
  return "UNKNOWN";
}

bool parse_trace_op(const std::string& text, TraceOp& out) {
  if (text == "WRITE" || text == "write") { out = TraceOp::Write; return true; }
  if (text == "READ" || text == "read") { out = TraceOp::Read; return true; }
  if (text == "TRIM" || text == "trim") { out = TraceOp::Trim; return true; }
  if (text == "FLUSH" || text == "flush") { out = TraceOp::Flush; return true; }
  return false;
}

std::vector<TraceRecord> TraceParser::load_from_file(const std::string& path) {
  std::vector<TraceRecord> records;
  std::ifstream fin(path);
  if (!fin.is_open()) {
    throw std::runtime_error("Could not open trace file: " + path);
  }

  std::string line;
  while (std::getline(fin, line)) {
    if (line.empty()) continue;
    auto j = nlohmann::json::parse(line);

    TraceRecord rec;
    rec.seq = j.value("seq", 0);
    std::string op_str = j.value("op", "WRITE");
    parse_trace_op(op_str, rec.op);
    rec.lba = j.value("lba", 0);

    if (rec.op == TraceOp::Write) {
      if (j.contains("data_hex")) {
        std::string hex = j["data_hex"].get<std::string>();
        rec.data.resize(hex.size() / 2);
        for (std::size_t i = 0; i < rec.data.size(); ++i) {
          std::string byte_str = hex.substr(i * 2, 2);
          rec.data[i] = static_cast<std::uint8_t>(std::stoul(byte_str, nullptr, 16));
        }
      } else if (j.contains("data_pattern")) {
        std::uint32_t val = j["data_pattern"].get<std::uint32_t>();
        std::size_t size = j.value("size", 4096);
        rec.data.resize(size);
        for (std::size_t i = 0; i < size; i += 4) {
          rec.data[i] = static_cast<std::uint8_t>(val & 0xFF);
          rec.data[i + 1] = static_cast<std::uint8_t>((val >> 8) & 0xFF);
          rec.data[i + 2] = static_cast<std::uint8_t>((val >> 16) & 0xFF);
          rec.data[i + 3] = static_cast<std::uint8_t>((val >> 24) & 0xFF);
        }
      } else {
        // Default deterministic payload if none provided
        rec.data.assign(4096, static_cast<std::uint8_t>(rec.lba & 0xFF));
      }
      rec.crc = crc32(rec.data.data(), rec.data.size());
    }

    records.push_back(rec);
  }

  return records;
}

void TraceParser::save_to_file(const std::string& path, const std::vector<TraceRecord>& records) {
  std::ofstream fout(path);
  if (!fout.is_open()) {
    throw std::runtime_error("Could not open file for writing trace: " + path);
  }

  for (const auto& rec : records) {
    nlohmann::json j;
    j["seq"] = rec.seq;
    j["op"] = to_string(rec.op);
    j["lba"] = rec.lba;

    if (rec.op == TraceOp::Write) {
      j["crc"] = crc32_hex(rec.crc);
      j["size"] = rec.data.size();
      if (!rec.data.empty()) {
        j["data_pattern"] = static_cast<std::uint32_t>(rec.data[0]);
      }
    }
    fout << j.dump() << "\n";
  }
}

}  // namespace ftl
