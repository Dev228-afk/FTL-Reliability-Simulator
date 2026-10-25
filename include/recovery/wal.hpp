#pragma once
// Write-Ahead Log (WAL) for FTL crash recovery.
// Ensures that every metadata change is durable BEFORE the host write is acknowledged.
// Each record is checksummed with CRC32 to detect torn writes caused by power loss.

#include <cstdint>
#include <string>
#include <vector>

#include "ftl/types.hpp"

namespace ftl {

enum class WalOp : std::uint8_t {
  Write = 1,
  Trim = 2,
  CheckpointMarker = 3,
  GcRelocate = 4,   // valid page copied by GC / static WL: lba now lives at ppa
  BlockErase = 5    // block index stored in `ppa`; appended only after the erase completed
};

#pragma pack(push, 1)
struct WalRecord {
  std::uint32_t magic = 0x57414C31;  // 'WAL1'
  std::uint8_t op = 0;                // WalOp
  std::uint32_t seq = 0;
  Lba lba = kInvalidLba;
  PpaIndex ppa = kInvalidPpa;
  std::uint32_t data_crc = 0;
  std::uint32_t record_crc = 0;       // CRC32 of bytes before record_crc
};
#pragma pack(pop)

class WalManager {
 public:
  WalManager() = default;

  void open(const std::string& filepath);
  void close();
  void clear();

  void append_write(std::uint32_t seq, Lba lba, PpaIndex ppa, std::uint32_t data_crc);
  void append_trim(std::uint32_t seq, Lba lba);
  void append_checkpoint_marker(std::uint32_t cp_seq);
  void append_relocate(std::uint32_t seq, Lba lba, PpaIndex new_ppa, std::uint32_t data_crc);
  void append_erase(std::uint32_t block_index);

  // Sync to durable storage
  void sync();

  // Replay all intact records from file
  std::vector<WalRecord> read_records() const;

  std::size_t record_count() const { return record_count_; }

 private:
  void append_record(WalOp op, std::uint32_t seq, Lba lba, PpaIndex ppa, std::uint32_t data_crc);

  std::string filepath_;
  std::size_t record_count_ = 0;
};

}  // namespace ftl
