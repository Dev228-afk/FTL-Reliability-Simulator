#include "ftl/config.hpp"

#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace ftl {

std::uint64_t SimConfig::logical_pages() const {
  const std::uint64_t total = geometry.total_pages();
  const double op_frac = geometry.overprovision_percent / 100.0;
  // User capacity derived from nominal OP percentage
  // user = total / (1 + op_frac)
  std::uint64_t user = static_cast<std::uint64_t>(std::floor(static_cast<double>(total) / (1.0 + op_frac)));
  // Capacity bound: must leave at least min_spare_blocks to guarantee GC forward progress
  const std::uint64_t max_allowed_user =
      (geometry.total_blocks() > min_spare_blocks)
          ? static_cast<std::uint64_t>(geometry.total_blocks() - min_spare_blocks) * geometry.pages_per_block
          : 0;
  if (user > max_allowed_user) {
    user = max_allowed_user;
  }
  return user > 0 ? user : 1;
}

double SimConfig::effective_op_percent() const {
  const double total = static_cast<double>(geometry.total_pages());
  const double user = static_cast<double>(logical_pages());
  if (user <= 0.0) return 0.0;
  return ((total - user) / user) * 100.0;
}

void SimConfig::validate() const {
  if (geometry.channels == 0) throw std::invalid_argument("channels must be > 0");
  if (geometry.dies_per_channel == 0) throw std::invalid_argument("dies_per_channel must be > 0");
  if (geometry.blocks_per_die < 4) throw std::invalid_argument("blocks_per_die must be >= 4");
  if (geometry.pages_per_block < 2) throw std::invalid_argument("pages_per_block must be >= 2");
  if (geometry.page_size_bytes == 0) throw std::invalid_argument("page_size_bytes must be > 0");
  if (geometry.overprovision_percent < 0.0) throw std::invalid_argument("overprovision_percent must be >= 0");
  if (min_spare_blocks < 2) throw std::invalid_argument("min_spare_blocks must be >= 2");
  if (geometry.total_blocks() <= min_spare_blocks) {
    throw std::invalid_argument("total_blocks must be strictly greater than min_spare_blocks");
  }
  if (timing.read_us == 0) throw std::invalid_argument("timing.read_us must be > 0");
  if (timing.program_us == 0) throw std::invalid_argument("timing.program_us must be > 0");
  if (timing.erase_us == 0) throw std::invalid_argument("timing.erase_us must be > 0");
  if (gc.low_watermark_blocks < 1) throw std::invalid_argument("gc.low_watermark_blocks must be >= 1");
}

nlohmann::json config_to_json(const SimConfig& cfg) {
  nlohmann::json j;
  j["geometry"]["channels"] = cfg.geometry.channels;
  j["geometry"]["dies_per_channel"] = cfg.geometry.dies_per_channel;
  j["geometry"]["blocks_per_die"] = cfg.geometry.blocks_per_die;
  j["geometry"]["pages_per_block"] = cfg.geometry.pages_per_block;
  j["geometry"]["page_size_bytes"] = cfg.geometry.page_size_bytes;
  j["geometry"]["overprovision_percent"] = cfg.geometry.overprovision_percent;

  j["timing"]["read_us"] = cfg.timing.read_us;
  j["timing"]["program_us"] = cfg.timing.program_us;
  j["timing"]["erase_us"] = cfg.timing.erase_us;
  j["timing"]["jitter_us"] = cfg.timing.jitter_us;
  j["timing"]["wear_penalty_us_per_kcycle"] = cfg.timing.wear_penalty_us_per_kcycle;

  j["gc"]["low_watermark_blocks"] = cfg.gc.low_watermark_blocks;
  j["gc"]["policy"] = to_string(cfg.gc.policy);

  j["wear"]["dynamic_enabled"] = cfg.wear.dynamic_enabled;
  j["wear"]["static_enabled"] = cfg.wear.static_enabled;
  j["wear"]["threshold"] = cfg.wear.threshold;
  j["wear"]["check_interval_erases"] = cfg.wear.check_interval_erases;

  j["persistence"]["mode"] = to_string(cfg.persistence.mode);
  j["persistence"]["checkpoint_interval_ops"] = cfg.persistence.checkpoint_interval_ops;
  j["persistence"]["checkpoint_on_flush"] = cfg.persistence.checkpoint_on_flush;

  j["debug"]["check_every_op"] = cfg.debug.check_every_op;
  j["debug"]["check_after_gc"] = cfg.debug.check_after_gc;
  j["debug"]["check_after_checkpoint"] = cfg.debug.check_after_checkpoint;

  j["seed"] = cfg.seed;
  j["min_spare_blocks"] = cfg.min_spare_blocks;
  return j;
}

SimConfig config_from_json(const nlohmann::json& j) {
  SimConfig cfg;
  if (!j.is_object()) throw std::invalid_argument("Config root must be a JSON object");

  for (auto it = j.begin(); it != j.end(); ++it) {
    const std::string& key = it.key();
    if (key == "geometry") {
      const auto& g = it.value();
      if (!g.is_object()) throw std::invalid_argument("geometry must be an object");
      for (auto git = g.begin(); git != g.end(); ++git) {
        const std::string& gk = git.key();
        if (gk == "channels") cfg.geometry.channels = git.value().get<std::uint32_t>();
        else if (gk == "dies_per_channel") cfg.geometry.dies_per_channel = git.value().get<std::uint32_t>();
        else if (gk == "blocks_per_die") cfg.geometry.blocks_per_die = git.value().get<std::uint32_t>();
        else if (gk == "pages_per_block") cfg.geometry.pages_per_block = git.value().get<std::uint32_t>();
        else if (gk == "page_size_bytes") cfg.geometry.page_size_bytes = git.value().get<std::uint32_t>();
        else if (gk == "overprovision_percent") cfg.geometry.overprovision_percent = git.value().get<double>();
        else throw std::invalid_argument("Unknown key in geometry: " + gk);
      }
    } else if (key == "timing") {
      const auto& t = it.value();
      if (!t.is_object()) throw std::invalid_argument("timing must be an object");
      for (auto tit = t.begin(); tit != t.end(); ++tit) {
        const std::string& tk = tit.key();
        if (tk == "read_us") cfg.timing.read_us = tit.value().get<std::uint32_t>();
        else if (tk == "program_us") cfg.timing.program_us = tit.value().get<std::uint32_t>();
        else if (tk == "erase_us") cfg.timing.erase_us = tit.value().get<std::uint32_t>();
        else if (tk == "jitter_us") cfg.timing.jitter_us = tit.value().get<std::uint32_t>();
        else if (tk == "wear_penalty_us_per_kcycle") cfg.timing.wear_penalty_us_per_kcycle = tit.value().get<double>();
        else throw std::invalid_argument("Unknown key in timing: " + tk);
      }
    } else if (key == "gc") {
      const auto& gc = it.value();
      if (!gc.is_object()) throw std::invalid_argument("gc must be an object");
      for (auto gcit = gc.begin(); gcit != gc.end(); ++gcit) {
        const std::string& gck = gcit.key();
        if (gck == "low_watermark_blocks") cfg.gc.low_watermark_blocks = gcit.value().get<std::uint32_t>();
        else if (gck == "policy") {
          std::string pol = gcit.value().get<std::string>();
          if (!parse_gc_policy(pol, cfg.gc.policy)) throw std::invalid_argument("Unknown gc.policy: " + pol);
        } else throw std::invalid_argument("Unknown key in gc: " + gck);
      }
    } else if (key == "wear") {
      const auto& w = it.value();
      if (!w.is_object()) throw std::invalid_argument("wear must be an object");
      for (auto wit = w.begin(); wit != w.end(); ++wit) {
        const std::string& wk = wit.key();
        if (wk == "dynamic_enabled") cfg.wear.dynamic_enabled = wit.value().get<bool>();
        else if (wk == "static_enabled") cfg.wear.static_enabled = wit.value().get<bool>();
        else if (wk == "threshold") cfg.wear.threshold = wit.value().get<std::uint32_t>();
        else if (wk == "check_interval_erases") cfg.wear.check_interval_erases = wit.value().get<std::uint32_t>();
        else throw std::invalid_argument("Unknown key in wear: " + wk);
      }
    } else if (key == "persistence") {
      const auto& p = it.value();
      if (!p.is_object()) throw std::invalid_argument("persistence must be an object");
      for (auto pit = p.begin(); pit != p.end(); ++pit) {
        const std::string& pk = pit.key();
        if (pk == "mode") {
          std::string m = pit.value().get<std::string>();
          if (!parse_recovery_mode(m, cfg.persistence.mode)) throw std::invalid_argument("Unknown persistence.mode: " + m);
        } else if (pk == "checkpoint_interval_ops") {
          cfg.persistence.checkpoint_interval_ops = pit.value().get<std::uint32_t>();
        } else if (pk == "checkpoint_on_flush") {
          cfg.persistence.checkpoint_on_flush = pit.value().get<bool>();
        } else throw std::invalid_argument("Unknown key in persistence: " + pk);
      }
    } else if (key == "debug") {
      const auto& d = it.value();
      if (!d.is_object()) throw std::invalid_argument("debug must be an object");
      for (auto dit = d.begin(); dit != d.end(); ++dit) {
        const std::string& dk = dit.key();
        if (dk == "check_every_op") cfg.debug.check_every_op = dit.value().get<bool>();
        else if (dk == "check_after_gc") cfg.debug.check_after_gc = dit.value().get<bool>();
        else if (dk == "check_after_checkpoint") cfg.debug.check_after_checkpoint = dit.value().get<bool>();
        else throw std::invalid_argument("Unknown key in debug: " + dk);
      }
    } else if (key == "seed") {
      cfg.seed = it.value().get<std::uint64_t>();
    } else if (key == "min_spare_blocks") {
      cfg.min_spare_blocks = it.value().get<std::uint32_t>();
    } else {
      throw std::invalid_argument("Unknown top-level config key: " + key);
    }
  }

  cfg.validate();
  return cfg;
}

static std::string trim(const std::string& str) {
  const auto start = str.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  const auto end = str.find_last_not_of(" \t\r\n");
  return str.substr(start, end - start + 1);
}

static nlohmann::json parse_value(const std::string& raw) {
  std::string s = trim(raw);
  if (s == "true") return true;
  if (s == "false") return false;
  if (s == "null") return nullptr;
  if (!s.empty() && (s.front() == '"' || s.front() == '\'')) {
    if (s.size() >= 2 && s.back() == s.front()) {
      return s.substr(1, s.size() - 2);
    }
  }
  // Try integer
  char* end_ptr = nullptr;
  long long ival = std::strtoll(s.c_str(), &end_ptr, 10);
  if (end_ptr != nullptr && *end_ptr == '\0') {
    return ival;
  }
  // Try double
  double dval = std::strtod(s.c_str(), &end_ptr);
  if (end_ptr != nullptr && *end_ptr == '\0') {
    return dval;
  }
  return s;
}

nlohmann::json parse_simple_yaml(const std::string& text) {
  nlohmann::json root = nlohmann::json::object();
  std::istringstream stream(text);
  std::string line;
  std::string current_section;

  while (std::getline(stream, line)) {
    // Strip comments
    auto comment_pos = line.find('#');
    if (comment_pos != std::string::npos) {
      line = line.substr(0, comment_pos);
    }
    if (trim(line).empty()) continue;

    // Check indentation
    size_t indent = line.find_first_not_of(" \t");
    std::string trimmed = trim(line);
    auto colon_pos = trimmed.find(':');
    if (colon_pos == std::string::npos) continue;

    std::string key = trim(trimmed.substr(0, colon_pos));
    std::string val_str = trim(trimmed.substr(colon_pos + 1));

    if (indent == 0) {
      if (val_str.empty()) {
        current_section = key;
        if (!root.contains(current_section)) {
          root[current_section] = nlohmann::json::object();
        }
      } else {
        current_section.clear();
        root[key] = parse_value(val_str);
      }
    } else {
      if (!current_section.empty()) {
        root[current_section][key] = parse_value(val_str);
      } else {
        root[key] = parse_value(val_str);
      }
    }
  }
  return root;
}

SimConfig load_config_file(const std::string& path) {
  std::ifstream fin(path);
  if (!fin.is_open()) {
    throw std::runtime_error("Failed to open config file: " + path);
  }
  std::stringstream ss;
  ss << fin.rdbuf();
  std::string content = ss.str();

  bool is_yaml = false;
  if (path.size() >= 5 && path.substr(path.size() - 5) == ".yaml") is_yaml = true;
  if (path.size() >= 4 && path.substr(path.size() - 4) == ".yml") is_yaml = true;

  nlohmann::json j;
  if (is_yaml) {
    j = parse_simple_yaml(content);
  } else {
    j = nlohmann::json::parse(content);
  }
  return config_from_json(j);
}

}  // namespace ftl
