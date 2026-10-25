#include "recovery/wal.hpp"

#include <cstdio>
#include <fstream>
#include <stdexcept>

#include "ftl/checksum.hpp"

namespace ftl {

void WalManager::open(const std::string& filepath) {
  filepath_ = filepath;
  // If file doesn't exist, create it
  std::ofstream fout(filepath_, std::ios::binary | std::ios::app);
}

void WalManager::close() {
  filepath_.clear();
  record_count_ = 0;
}

void WalManager::clear() {
  if (!filepath_.empty()) {
    std::ofstream fout(filepath_, std::ios::binary | std::ios::trunc);
  }
  record_count_ = 0;
}

void WalManager::append_record(WalOp op, std::uint32_t seq, Lba lba, PpaIndex ppa, std::uint32_t data_crc) {
  if (filepath_.empty()) return;

  WalRecord rec;
  rec.magic = 0x57414C31;
  rec.op = static_cast<std::uint8_t>(op);
  rec.seq = seq;
  rec.lba = lba;
  rec.ppa = ppa;
  rec.data_crc = data_crc;

  const std::size_t crc_payload_len = sizeof(WalRecord) - sizeof(rec.record_crc);
  rec.record_crc = crc32(&rec, crc_payload_len);

  std::ofstream fout(filepath_, std::ios::binary | std::ios::app);
  if (!fout.is_open()) throw std::runtime_error("Failed to open WAL file for append: " + filepath_);
  fout.write(reinterpret_cast<const char*>(&rec), sizeof(rec));
  fout.flush();
  if (!fout.good()) throw std::runtime_error("Failed to append WAL record: " + filepath_);
  record_count_++;
}

void WalManager::append_write(std::uint32_t seq, Lba lba, PpaIndex ppa, std::uint32_t data_crc) {
  append_record(WalOp::Write, seq, lba, ppa, data_crc);
}

void WalManager::append_trim(std::uint32_t seq, Lba lba) {
  append_record(WalOp::Trim, seq, lba, kInvalidPpa, 0);
}

void WalManager::append_checkpoint_marker(std::uint32_t cp_seq) {
  append_record(WalOp::CheckpointMarker, cp_seq, kInvalidLba, kInvalidPpa, 0);
}

void WalManager::append_relocate(std::uint32_t seq, Lba lba, PpaIndex new_ppa, std::uint32_t data_crc) {
  append_record(WalOp::GcRelocate, seq, lba, new_ppa, data_crc);
}

void WalManager::append_erase(std::uint32_t block_index) {
  append_record(WalOp::BlockErase, 0, kInvalidLba, static_cast<PpaIndex>(block_index), 0);
}

void WalManager::sync() {
  // std::ofstream::flush is called on every append.
}

std::vector<WalRecord> WalManager::read_records() const {
  std::vector<WalRecord> records;
  if (filepath_.empty()) return records;

  std::ifstream fin(filepath_, std::ios::binary);
  if (!fin.is_open()) return records;

  WalRecord rec;
  while (fin.read(reinterpret_cast<char*>(&rec), sizeof(rec))) {
    if (rec.magic != 0x57414C31) {
      // Corrupt magic, stop replay
      break;
    }
    const std::size_t crc_payload_len = sizeof(WalRecord) - sizeof(rec.record_crc);
    std::uint32_t computed = crc32(&rec, crc_payload_len);
    if (computed != rec.record_crc) {
      // Torn record, stop replay
      break;
    }
    records.push_back(rec);
  }
  return records;
}

}  // namespace ftl
