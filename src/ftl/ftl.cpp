#include "ftl/ftl.hpp"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <stdexcept>

#include "ftl/checksum.hpp"
#include "invariants/invariants.hpp"

namespace ftl {

double FtlStats::write_amplification() const {
  if (host_writes == 0) return 1.0;
  // Total NAND pages programmed = host writes + GC relocated pages
  const double total_nand_writes = static_cast<double>(host_writes + gc_pages_relocated);
  return total_nand_writes / static_cast<double>(host_writes);
}

Ftl::Ftl(const SimConfig& config, const std::string& work_dir)
    : config_(config),
      work_dir_(work_dir),
      nand_(config),
      rng_(config.seed) {
  std::filesystem::create_directories(work_dir_);

  const auto logical_p = config_.logical_pages();
  const auto physical_p = config_.geometry.total_pages();
  const auto total_b = config_.geometry.total_blocks();
  const auto ppb = config_.geometry.pages_per_block;

  l2p_.assign(logical_p, kInvalidPpa);
  p2l_.assign(physical_p, kInvalidLba);
  page_states_.assign(physical_p, PageState::Free);
  block_states_.assign(total_b, BlockState::Free);

  valid_pages_.assign(total_b, 0);
  invalid_pages_.assign(total_b, 0);
  free_pages_.assign(total_b, ppb);

  if (config_.persistence.mode == RecoveryMode::Wal) {
    wal_.open(work_dir_ + "/wal.log");
  }

  std::string nand_path = work_dir_ + "/nand.bin";
  if (std::filesystem::exists(nand_path)) {
    nand_.load_from_file(nand_path);
  }
}

std::uint32_t Ftl::count_free_blocks() const {
  std::uint32_t count = 0;
  for (const auto& st : block_states_) {
    if (st == BlockState::Free) count++;
  }
  return count;
}

std::uint32_t Ftl::pick_free_block(bool gc_block) {
  const auto total_b = config_.geometry.total_blocks();
  std::uint32_t best_block = kInvalidPpa;
  std::uint32_t min_erase = std::numeric_limits<std::uint32_t>::max();

  for (std::uint32_t b = 0; b < total_b; ++b) {
    if (block_states_[b] == BlockState::Free) {
      std::uint32_t ec = nand_.get_erase_count(b);
      if (config_.wear.dynamic_enabled) {
        if (ec < min_erase) {
          min_erase = ec;
          best_block = b;
        }
      } else {
        best_block = b;
        break;
      }
    }
  }

  if (best_block == kInvalidPpa) {
    throw std::runtime_error("Out of free blocks: NAND capacity exhausted");
  }

  block_states_[best_block] = gc_block ? BlockState::OpenGc : BlockState::OpenUser;
  return best_block;
}

void Ftl::close_block(std::uint32_t block_index) {
  if (block_index != kInvalidPpa) {
    block_states_[block_index] = BlockState::Closed;
  }
}

PpaIndex Ftl::allocate_user_page() {
  const auto ppb = config_.geometry.pages_per_block;
  if (user_open_block_ == kInvalidPpa || user_next_page_ >= ppb) {
    close_block(user_open_block_);
    user_open_block_ = pick_free_block(false);
    user_next_page_ = 0;
  }

  const PpaIndex ppa = user_open_block_ * ppb + user_next_page_++;
  page_states_[ppa] = PageState::Valid;
  free_pages_[user_open_block_]--;
  valid_pages_[user_open_block_]++;
  return ppa;
}

PpaIndex Ftl::allocate_gc_page() {
  const auto ppb = config_.geometry.pages_per_block;
  if (gc_open_block_ == kInvalidPpa || gc_next_page_ >= ppb) {
    close_block(gc_open_block_);
    gc_open_block_ = pick_free_block(true);
    gc_next_page_ = 0;
  }

  const PpaIndex ppa = gc_open_block_ * ppb + gc_next_page_++;
  page_states_[ppa] = PageState::Valid;
  free_pages_[gc_open_block_]--;
  valid_pages_[gc_open_block_]++;
  return ppa;
}

void Ftl::write(Lba lba, const std::vector<std::uint8_t>& data, std::uint32_t seq) {
  if (lba >= config_.logical_pages()) {
    throw std::out_of_range("LBA out of bounds: " + std::to_string(lba));
  }

  // Ensure free block threshold before allocating
  if (count_free_blocks() <= config_.gc.low_watermark_blocks) {
    trigger_gc();
  }

  // 1. Invalidate previous physical page if previously mapped
  PpaIndex old_ppa = l2p_[lba];
  if (old_ppa != kInvalidPpa) {
    page_states_[old_ppa] = PageState::Invalid;
    p2l_[old_ppa] = kInvalidLba;
    std::uint32_t old_b = nand_.block_of_ppa(old_ppa);
    valid_pages_[old_b]--;
    invalid_pages_[old_b]++;
  }

  // 2. Allocate new physical page for user write
  PpaIndex new_ppa = allocate_user_page();

  // 3. Program NAND
  PageMetadata meta;
  meta.lba = lba;
  meta.seq = seq;
  meta.crc = crc32(data.data(), data.size());
  nand_.program_page(new_ppa, data, meta);
  nand_.save_to_file(work_dir_ + "/nand.bin");

  crash_.check(CrashPoint::AfterDataProgram, seq);

  // 4. WAL logging in WAL mode
  if (config_.persistence.mode == RecoveryMode::Wal) {
    wal_.append_write(seq, lba, new_ppa, meta.crc);
  }

  crash_.check(CrashPoint::BeforeMappingUpdate, seq);

  // 5. Update in-memory mapping
  l2p_[lba] = new_ppa;
  p2l_[new_ppa] = lba;

  crash_.check(CrashPoint::AfterMappingUpdate, seq);

  // 6. Host acknowledgement
  stats_.host_writes++;

  crash_.check(CrashPoint::AfterAck, seq);

  // Periodic maintenance
  ops_since_checkpoint_++;
  if (config_.persistence.checkpoint_interval_ops > 0 &&
      ops_since_checkpoint_ >= config_.persistence.checkpoint_interval_ops) {
    checkpoint();
    ops_since_checkpoint_ = 0;
  }

  if (config_.debug.check_every_op) {
    auto rep = InvariantChecker::check_structural(*this);
    if (!rep.passed) {
      throw std::runtime_error("Invariant violation after write: " + rep.violations[0].details);
    }
  }
}

bool Ftl::read(Lba lba, std::vector<std::uint8_t>& out_data, std::uint32_t /*seq*/) {
  if (lba >= config_.logical_pages()) {
    throw std::out_of_range("LBA out of bounds: " + std::to_string(lba));
  }
  stats_.host_reads++;

  PpaIndex ppa = l2p_[lba];
  if (ppa == kInvalidPpa) {
    // Unmapped read returns zeroes
    out_data.assign(config_.geometry.page_size_bytes, 0);
    return true;
  }

  PageMetadata meta;
  nand_.read_page(ppa, out_data, meta);

  // Verification of ownership and checksum
  if (meta.lba != lba) return false;
  std::uint32_t actual_crc = crc32(out_data.data(), out_data.size());
  return actual_crc == meta.crc;
}

void Ftl::trim(Lba lba, std::uint32_t seq) {
  if (lba >= config_.logical_pages()) {
    throw std::out_of_range("LBA out of bounds: " + std::to_string(lba));
  }
  stats_.host_trims++;

  PpaIndex old_ppa = l2p_[lba];
  if (old_ppa != kInvalidPpa) {
    page_states_[old_ppa] = PageState::Invalid;
    p2l_[old_ppa] = kInvalidLba;
    std::uint32_t old_b = nand_.block_of_ppa(old_ppa);
    valid_pages_[old_b]--;
    invalid_pages_[old_b]++;
    l2p_[lba] = kInvalidPpa;

    if (config_.persistence.mode == RecoveryMode::Wal) {
      wal_.append_trim(seq, lba);
    }
  }
}

void Ftl::flush(std::uint32_t /*seq*/) {
  if (config_.persistence.checkpoint_on_flush) {
    checkpoint();
  }
}

std::uint32_t Ftl::select_gc_victim() {
  const auto total_b = config_.geometry.total_blocks();
  std::uint32_t best_b = kInvalidPpa;
  std::uint32_t max_invalid = 0;
  double best_cost_benefit = -1.0;

  for (std::uint32_t b = 0; b < total_b; ++b) {
    // Only closed blocks can be reclaimed
    if (block_states_[b] != BlockState::Closed) continue;

    if (config_.gc.policy == GcPolicy::Greedy) {
      if (invalid_pages_[b] > max_invalid) {
        max_invalid = invalid_pages_[b];
        best_b = b;
      }
    } else {
      // Cost-Benefit policy: invalid_pages / (erase_count + 1)
      double cb = static_cast<double>(invalid_pages_[b]) / static_cast<double>(nand_.get_erase_count(b) + 1);
      if (cb > best_cost_benefit) {
        best_cost_benefit = cb;
        best_b = b;
      }
    }
  }

  // Fallback: pick any closed block with at least 1 invalid page, or any closed block
  if (best_b == kInvalidPpa) {
    for (std::uint32_t b = 0; b < total_b; ++b) {
      if (block_states_[b] == BlockState::Closed) {
        best_b = b;
        break;
      }
    }
  }

  return best_b;
}

bool Ftl::trigger_gc() {
  std::uint32_t victim = select_gc_victim();
  if (victim == kInvalidPpa) return false;

  const auto ppb = config_.geometry.pages_per_block;
  const PpaIndex start_ppa = victim * ppb;

  // 1. Relocate all valid pages in the victim block
  for (std::uint32_t p = 0; p < ppb; ++p) {
    PpaIndex old_ppa = start_ppa + p;
    if (page_states_[old_ppa] == PageState::Valid) {
      Lba lba = p2l_[old_ppa];
      if (lba != kInvalidLba && l2p_[lba] == old_ppa) {
        // Read valid data from NAND
        std::vector<std::uint8_t> data;
        PageMetadata meta;
        nand_.read_page(old_ppa, data, meta);

        // Allocate new GC page
        PpaIndex new_ppa = allocate_gc_page();

        crash_.check(CrashPoint::DuringGcCopy, meta.seq);

        // Program new GC page
        nand_.program_page(new_ppa, data, meta);

        // Remap LBA
        l2p_[lba] = new_ppa;
        p2l_[new_ppa] = lba;

        if (config_.persistence.mode == RecoveryMode::Wal) {
          wal_.append_relocate(meta.seq, lba, new_ppa, meta.crc);
        }

        stats_.gc_pages_relocated++;
      }
    }
  }

  crash_.check(CrashPoint::BeforeBlockErase, 0);

  // 2. Erase victim block
  nand_.erase_block(victim);
  block_states_[victim] = BlockState::Free;
  valid_pages_[victim] = 0;
  invalid_pages_[victim] = 0;
  free_pages_[victim] = ppb;
  for (std::uint32_t p = 0; p < ppb; ++p) {
    PpaIndex ppa = start_ppa + p;
    page_states_[ppa] = PageState::Free;
    p2l_[ppa] = kInvalidLba;
  }
  nand_.save_to_file(work_dir_ + "/nand.bin");

  if (config_.persistence.mode == RecoveryMode::Wal) {
    wal_.append_erase(victim);
  }

  stats_.gc_cycles++;
  erases_since_wear_check_++;

  if (config_.debug.check_after_gc) {
    auto rep = InvariantChecker::check_structural(*this);
    if (!rep.passed) {
      throw std::runtime_error("Invariant violation after GC: " + rep.violations[0].details);
    }
  }

  return true;
}

void Ftl::perform_static_wear_leveling() {
  if (!config_.wear.static_enabled) return;

  auto dist = nand_.calculate_wear_distribution();
  if (dist.max_erase - dist.min_erase < config_.wear.threshold) return;

  // Find the block with minimum erase count that contains cold data (valid pages)
  const auto total_b = config_.geometry.total_blocks();
  std::uint32_t cold_block = kInvalidPpa;
  std::uint32_t min_e = std::numeric_limits<std::uint32_t>::max();

  for (std::uint32_t b = 0; b < total_b; ++b) {
    if (block_states_[b] == BlockState::Closed && valid_pages_[b] > 0) {
      std::uint32_t ec = nand_.get_erase_count(b);
      if (ec < min_e) {
        min_e = ec;
        cold_block = b;
      }
    }
  }

  if (cold_block == kInvalidPpa) return;

  // Relocate cold data off this block and erase it to balance wear
  const auto ppb = config_.geometry.pages_per_block;
  const PpaIndex start_ppa = cold_block * ppb;
  for (std::uint32_t p = 0; p < ppb; ++p) {
    PpaIndex old_ppa = start_ppa + p;
    if (page_states_[old_ppa] == PageState::Valid) {
      Lba lba = p2l_[old_ppa];
      if (lba != kInvalidLba && l2p_[lba] == old_ppa) {
        std::vector<std::uint8_t> data;
        PageMetadata meta;
        nand_.read_page(old_ppa, data, meta);

        PpaIndex new_ppa = allocate_gc_page();
        nand_.program_page(new_ppa, data, meta);

        l2p_[lba] = new_ppa;
        p2l_[new_ppa] = lba;

        if (config_.persistence.mode == RecoveryMode::Wal) {
          wal_.append_relocate(meta.seq, lba, new_ppa, meta.crc);
        }
      }
    }
  }

  nand_.erase_block(cold_block);
  block_states_[cold_block] = BlockState::Free;
  valid_pages_[cold_block] = 0;
  invalid_pages_[cold_block] = 0;
  free_pages_[cold_block] = ppb;
  for (std::uint32_t p = 0; p < ppb; ++p) {
    PpaIndex ppa = start_ppa + p;
    page_states_[ppa] = PageState::Free;
    p2l_[ppa] = kInvalidLba;
  }
  nand_.save_to_file(work_dir_ + "/nand.bin");

  if (config_.persistence.mode == RecoveryMode::Wal) {
    wal_.append_erase(cold_block);
  }

  stats_.static_wear_level_events++;
}

void Ftl::checkpoint() {
  crash_.check(CrashPoint::DuringCheckpoint, checkpoint_seq_);

  CheckpointData data;
  data.checkpoint_seq = ++checkpoint_seq_;
  data.logical_pages = config_.logical_pages();
  data.physical_pages = config_.geometry.total_pages();
  data.total_blocks = config_.geometry.total_blocks();

  data.l2p = l2p_;
  data.p2l = p2l_;
  data.page_states.resize(page_states_.size());
  for (std::size_t i = 0; i < page_states_.size(); ++i) {
    data.page_states[i] = static_cast<std::uint8_t>(page_states_[i]);
  }

  data.erase_counts.resize(data.total_blocks);
  data.block_states.resize(data.total_blocks);
  for (std::uint32_t b = 0; b < data.total_blocks; ++b) {
    data.erase_counts[b] = nand_.get_erase_count(b);
    data.block_states[b] = static_cast<std::uint8_t>(block_states_[b]);
  }

  data.user_open_block = user_open_block_;
  data.user_next_page = user_next_page_;
  data.gc_open_block = gc_open_block_;
  data.gc_next_page = gc_next_page_;

  std::string cp_path = work_dir_ + "/checkpoint.meta";
  if (!CheckpointManager::save(cp_path, data)) {
    throw std::runtime_error("Failed to write checkpoint to " + cp_path);
  }

  if (config_.persistence.mode == RecoveryMode::Wal) {
    wal_.append_checkpoint_marker(checkpoint_seq_);
  }

  if (config_.debug.check_after_checkpoint) {
    auto rep = InvariantChecker::check_structural(*this);
    if (!rep.passed) {
      throw std::runtime_error("Invariant violation after checkpoint: " + rep.violations[0].details);
    }
  }
}

bool Ftl::recover() {
  std::string cp_path = work_dir_ + "/checkpoint.meta";
  CheckpointData data;
  bool loaded_cp = CheckpointManager::load(cp_path, data);

  if (loaded_cp) {
    checkpoint_seq_ = data.checkpoint_seq;
    l2p_ = data.l2p;
    p2l_ = data.p2l;

    page_states_.resize(data.page_states.size());
    for (std::size_t i = 0; i < page_states_.size(); ++i) {
      page_states_[i] = static_cast<PageState>(data.page_states[i]);
    }

    const auto total_b = config_.geometry.total_blocks();
    block_states_.resize(total_b);
    for (std::uint32_t b = 0; b < total_b; ++b) {
      block_states_[b] = static_cast<BlockState>(data.block_states[b]);
      nand_.set_erase_count(b, data.erase_counts[b]);
    }

    user_open_block_ = data.user_open_block;
    user_next_page_ = data.user_next_page;
    gc_open_block_ = data.gc_open_block;
    gc_next_page_ = data.gc_next_page;

    // Recalculate block counters
    const auto ppb = config_.geometry.pages_per_block;
    for (std::uint32_t b = 0; b < total_b; ++b) {
      valid_pages_[b] = 0;
      invalid_pages_[b] = 0;
      free_pages_[b] = 0;
      const PpaIndex start_ppa = b * ppb;
      for (std::uint32_t p = 0; p < ppb; ++p) {
        PageState st = page_states_[start_ppa + p];
        if (st == PageState::Valid) valid_pages_[b]++;
        else if (st == PageState::Invalid) invalid_pages_[b]++;
        else if (st == PageState::Free) free_pages_[b]++;
      }
    }
  }

  // WAL replay in WAL mode:
  if (config_.persistence.mode == RecoveryMode::Wal) {
    auto records = wal_.read_records();
    for (const auto& rec : records) {
      if (rec.op == static_cast<std::uint8_t>(WalOp::Write) ||
          rec.op == static_cast<std::uint8_t>(WalOp::GcRelocate)) {
        // Reapply write or GC relocation
        Lba lba = rec.lba;
        PpaIndex new_ppa = rec.ppa;
        if (lba < l2p_.size() && new_ppa < p2l_.size()) {
          PpaIndex old_ppa = l2p_[lba];
          if (old_ppa != kInvalidPpa && old_ppa != new_ppa) {
            page_states_[old_ppa] = PageState::Invalid;
            p2l_[old_ppa] = kInvalidLba;
            std::uint32_t old_b = nand_.block_of_ppa(old_ppa);
            if (valid_pages_[old_b] > 0) valid_pages_[old_b]--;
            invalid_pages_[old_b]++;
          }

          l2p_[lba] = new_ppa;
          p2l_[new_ppa] = lba;
          page_states_[new_ppa] = PageState::Valid;

          std::uint32_t new_b = nand_.block_of_ppa(new_ppa);
          if (free_pages_[new_b] > 0) free_pages_[new_b]--;
          valid_pages_[new_b]++;
        }
      } else if (rec.op == static_cast<std::uint8_t>(WalOp::Trim)) {
        Lba lba = rec.lba;
        if (lba < l2p_.size()) {
          PpaIndex old_ppa = l2p_[lba];
          if (old_ppa != kInvalidPpa) {
            page_states_[old_ppa] = PageState::Invalid;
            p2l_[old_ppa] = kInvalidLba;
            std::uint32_t old_b = nand_.block_of_ppa(old_ppa);
            if (valid_pages_[old_b] > 0) valid_pages_[old_b]--;
            invalid_pages_[old_b]++;
            l2p_[lba] = kInvalidPpa;
          }
        }
      } else if (rec.op == static_cast<std::uint8_t>(WalOp::BlockErase)) {
        std::uint32_t b = static_cast<std::uint32_t>(rec.ppa);
        const auto total_b = config_.geometry.total_blocks();
        const auto ppb = config_.geometry.pages_per_block;
        if (b < total_b) {
          block_states_[b] = BlockState::Free;
          valid_pages_[b] = 0;
          invalid_pages_[b] = 0;
          free_pages_[b] = ppb;
          const PpaIndex start_ppa = b * ppb;
          for (std::uint32_t p = 0; p < ppb; ++p) {
            PpaIndex ppa = start_ppa + p;
            page_states_[ppa] = PageState::Free;
            p2l_[ppa] = kInvalidLba;
          }
        }
      }
    }

    // Recompute block counters and states after WAL replay
    const auto total_b = config_.geometry.total_blocks();
    const auto ppb = config_.geometry.pages_per_block;
    for (std::uint32_t b = 0; b < total_b; ++b) {
      valid_pages_[b] = 0;
      invalid_pages_[b] = 0;
      free_pages_[b] = 0;
      const PpaIndex start_ppa = b * ppb;
      for (std::uint32_t p = 0; p < ppb; ++p) {
        PageState st = page_states_[start_ppa + p];
        if (st == PageState::Valid) valid_pages_[b]++;
        else if (st == PageState::Invalid) invalid_pages_[b]++;
        else if (st == PageState::Free) free_pages_[b]++;
      }
      if (block_states_[b] == BlockState::Free && (valid_pages_[b] > 0 || invalid_pages_[b] > 0)) {
        block_states_[b] = (free_pages_[b] == 0) ? BlockState::Closed : BlockState::OpenUser;
      }
    }
  }

  std::string nand_path = work_dir_ + "/nand.bin";
  if (std::filesystem::exists(nand_path)) {
    nand_.load_from_file(nand_path);
  }

  return loaded_cp || config_.persistence.mode == RecoveryMode::Wal;
}

}  // namespace ftl
