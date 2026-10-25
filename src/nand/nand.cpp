#include "nand/nand.hpp"

#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace ftl {

NandModel::NandModel(const SimConfig& config)
    : config_(config), rng_(config.seed) {
  const std::size_t total_pages = static_cast<std::size_t>(config_.geometry.total_pages());
  const std::size_t total_blocks = static_cast<std::size_t>(config_.geometry.total_blocks());

  programmed_.assign(total_pages, false);
  storage_.resize(total_pages);
  metadata_.resize(total_pages);
  blocks_.resize(total_blocks);
}

Ppa NandModel::decode_ppa(PpaIndex ppa) const {
  const auto ppb = config_.geometry.pages_per_block;
  const auto bpd = config_.geometry.blocks_per_die;
  const auto dpc = config_.geometry.dies_per_channel;

  Ppa d;
  d.page = ppa % ppb;
  std::uint32_t b_idx = ppa / ppb;
  d.block = b_idx % bpd;
  std::uint32_t die_idx = b_idx / bpd;
  d.die = die_idx % dpc;
  d.channel = die_idx / dpc;
  return d;
}

PpaIndex NandModel::encode_ppa(const Ppa& decoded) const {
  const auto ppb = config_.geometry.pages_per_block;
  const auto bpd = config_.geometry.blocks_per_die;
  const auto dpc = config_.geometry.dies_per_channel;

  std::uint32_t block_flat = (decoded.channel * dpc + decoded.die) * bpd + decoded.block;
  return block_flat * ppb + decoded.page;
}

std::uint32_t NandModel::block_of_ppa(PpaIndex ppa) const {
  return ppa / config_.geometry.pages_per_block;
}

std::uint32_t NandModel::page_in_block(PpaIndex ppa) const {
  return ppa % config_.geometry.pages_per_block;
}

bool NandModel::is_page_programmed(PpaIndex ppa) const {
  if (ppa >= programmed_.size()) {
    throw std::out_of_range("PPA index out of bounds in is_page_programmed");
  }
  return programmed_[ppa];
}

void NandModel::program_page(PpaIndex ppa, const std::vector<std::uint8_t>& data, const PageMetadata& meta) {
  if (ppa >= programmed_.size()) {
    throw std::out_of_range("PPA index out of bounds in program_page");
  }
  if (programmed_[ppa]) {
    throw std::runtime_error("NAND violation: attempt to overwrite page without erasing block first");
  }

  programmed_[ppa] = true;
  storage_[ppa] = data;
  metadata_[ppa] = meta;

  std::uint32_t b_idx = block_of_ppa(ppa);
  blocks_[b_idx].programmed_pages++;

  stats_.total_programs++;
  account_latency(config_.timing.program_us, blocks_[b_idx].erase_count);
}

void NandModel::read_page(PpaIndex ppa, std::vector<std::uint8_t>& out_data, PageMetadata& out_meta) {
  if (ppa >= programmed_.size()) {
    throw std::out_of_range("PPA index out of bounds in read_page");
  }
  if (!programmed_[ppa]) {
    // Reading an erased page yields empty/all 0xFF (or uninitialized)
    out_data.assign(config_.geometry.page_size_bytes, 0xFF);
    out_meta = PageMetadata{};
  } else {
    out_data = storage_[ppa];
    out_meta = metadata_[ppa];
  }

  std::uint32_t b_idx = block_of_ppa(ppa);
  stats_.total_reads++;
  account_latency(config_.timing.read_us, blocks_[b_idx].erase_count);
}

void NandModel::erase_block(std::uint32_t block_index) {
  if (block_index >= blocks_.size()) {
    throw std::out_of_range("Block index out of bounds in erase_block");
  }

  const auto ppb = config_.geometry.pages_per_block;
  const PpaIndex start_ppa = block_index * ppb;
  for (std::uint32_t i = 0; i < ppb; ++i) {
    const PpaIndex ppa = start_ppa + i;
    programmed_[ppa] = false;
    storage_[ppa].clear();
    metadata_[ppa] = PageMetadata{};
  }

  blocks_[block_index].programmed_pages = 0;
  blocks_[block_index].erase_count++;

  stats_.total_erases++;
  account_latency(config_.timing.erase_us, blocks_[block_index].erase_count);
}

void NandModel::dump_raw(std::vector<std::uint8_t>& out_bytes) const {
  out_bytes.clear();
  const std::uint32_t total_blocks = static_cast<std::uint32_t>(blocks_.size());
  for (std::uint32_t i = 0; i < total_blocks; ++i) {
    std::uint32_t ec = blocks_[i].erase_count;
    out_bytes.insert(out_bytes.end(), reinterpret_cast<const std::uint8_t*>(&ec),
                     reinterpret_cast<const std::uint8_t*>(&ec) + sizeof(ec));
  }
}

void NandModel::load_raw(const std::vector<std::uint8_t>& in_bytes) {
  const std::size_t total_blocks = blocks_.size();
  if (in_bytes.size() >= total_blocks * sizeof(std::uint32_t)) {
    const auto* p = reinterpret_cast<const std::uint32_t*>(in_bytes.data());
    for (std::size_t i = 0; i < total_blocks; ++i) {
      blocks_[i].erase_count = p[i];
    }
  }
}

bool NandModel::save_to_file(const std::string& path) const {
  std::ofstream fout(path, std::ios::binary | std::ios::trunc);
  if (!fout.is_open()) return false;

  std::uint32_t magic = 0x4E414E44;  // 'NAND'
  std::uint64_t total_pages = storage_.size();
  std::uint32_t page_size = config_.geometry.page_size_bytes;
  std::uint32_t total_blocks = blocks_.size();

  fout.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
  fout.write(reinterpret_cast<const char*>(&total_pages), sizeof(total_pages));
  fout.write(reinterpret_cast<const char*>(&page_size), sizeof(page_size));
  fout.write(reinterpret_cast<const char*>(&total_blocks), sizeof(total_blocks));

  for (std::size_t i = 0; i < total_pages; ++i) {
    std::uint8_t prog = programmed_[i] ? 1 : 0;
    fout.write(reinterpret_cast<const char*>(&prog), sizeof(prog));
    fout.write(reinterpret_cast<const char*>(&metadata_[i]), sizeof(PageMetadata));
    if (prog) {
      if (storage_[i].size() == page_size) {
        fout.write(reinterpret_cast<const char*>(storage_[i].data()), page_size);
      } else {
        std::vector<char> pad(page_size, 0);
        fout.write(pad.data(), page_size);
      }
    }
  }

  for (std::size_t i = 0; i < total_blocks; ++i) {
    fout.write(reinterpret_cast<const char*>(&blocks_[i].erase_count), sizeof(std::uint32_t));
    fout.write(reinterpret_cast<const char*>(&blocks_[i].programmed_pages), sizeof(std::uint32_t));
  }

  fout.flush();
  return fout.good();
}

bool NandModel::load_from_file(const std::string& path) {
  std::ifstream fin(path, std::ios::binary);
  if (!fin.is_open()) return false;

  std::uint32_t magic = 0;
  std::uint64_t total_pages = 0;
  std::uint32_t page_size = 0;
  std::uint32_t total_blocks = 0;

  if (!fin.read(reinterpret_cast<char*>(&magic), sizeof(magic)) || magic != 0x4E414E44) return false;
  if (!fin.read(reinterpret_cast<char*>(&total_pages), sizeof(total_pages))) return false;
  if (!fin.read(reinterpret_cast<char*>(&page_size), sizeof(page_size))) return false;
  if (!fin.read(reinterpret_cast<char*>(&total_blocks), sizeof(total_blocks))) return false;

  if (total_pages != storage_.size() || total_blocks != blocks_.size()) return false;

  for (std::size_t i = 0; i < total_pages; ++i) {
    std::uint8_t prog = 0;
    fin.read(reinterpret_cast<char*>(&prog), sizeof(prog));
    programmed_[i] = (prog != 0);
    fin.read(reinterpret_cast<char*>(&metadata_[i]), sizeof(PageMetadata));
    if (prog) {
      storage_[i].resize(page_size);
      fin.read(reinterpret_cast<char*>(storage_[i].data()), page_size);
    } else {
      storage_[i].clear();
    }
  }

  for (std::size_t i = 0; i < total_blocks; ++i) {
    fin.read(reinterpret_cast<char*>(&blocks_[i].erase_count), sizeof(std::uint32_t));
    fin.read(reinterpret_cast<char*>(&blocks_[i].programmed_pages), sizeof(std::uint32_t));
  }

  return true;
}

std::uint32_t NandModel::get_erase_count(std::uint32_t block_index) const {
  if (block_index >= blocks_.size()) {
    throw std::out_of_range("Block index out of bounds");
  }
  return blocks_[block_index].erase_count;
}

void NandModel::set_erase_count(std::uint32_t block_index, std::uint32_t count) {
  if (block_index >= blocks_.size()) {
    throw std::out_of_range("Block index out of bounds");
  }
  blocks_[block_index].erase_count = count;
}

WearDistribution NandModel::calculate_wear_distribution() const {
  if (blocks_.empty()) return WearDistribution{};

  std::uint32_t min_e = blocks_[0].erase_count;
  std::uint32_t max_e = blocks_[0].erase_count;
  double sum = 0.0;

  for (const auto& b : blocks_) {
    if (b.erase_count < min_e) min_e = b.erase_count;
    if (b.erase_count > max_e) max_e = b.erase_count;
    sum += b.erase_count;
  }

  const double mean = sum / static_cast<double>(blocks_.size());
  double var_sum = 0.0;
  for (const auto& b : blocks_) {
    double diff = static_cast<double>(b.erase_count) - mean;
    var_sum += diff * diff;
  }
  const double stddev = std::sqrt(var_sum / static_cast<double>(blocks_.size()));

  return WearDistribution{min_e, max_e, mean, stddev};
}

void NandModel::reset_stats() {
  stats_ = NandStats{};
}

void NandModel::account_latency(std::uint32_t base_us, double wear_cycles) {
  double latency = static_cast<double>(base_us);
  if (config_.timing.jitter_us > 0) {
    latency += static_cast<double>(rng_.uniform(config_.timing.jitter_us + 1));
  }
  if (config_.timing.wear_penalty_us_per_kcycle > 0.0) {
    double kcycles = wear_cycles / 1000.0;
    latency += kcycles * config_.timing.wear_penalty_us_per_kcycle;
  }
  stats_.simulated_time_us += static_cast<std::uint64_t>(std::round(latency));
}

}  // namespace ftl
