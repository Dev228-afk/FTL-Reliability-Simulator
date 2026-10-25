#include "recovery/checkpoint.hpp"

#include <cstring>
#include <fstream>
#include <vector>

#include "ftl/checksum.hpp"

namespace ftl {

#pragma pack(push, 1)
struct CheckpointHeader {
  std::uint32_t magic = 0x43504E54;  // 'CPNT'
  std::uint32_t checkpoint_seq = 0;
  std::uint64_t logical_pages = 0;
  std::uint64_t physical_pages = 0;
  std::uint32_t total_blocks = 0;
  std::uint32_t user_open_block = 0;
  std::uint32_t user_next_page = 0;
  std::uint32_t gc_open_block = 0;
  std::uint32_t gc_next_page = 0;
  std::uint32_t payload_bytes = 0;
  std::uint32_t checksum = 0;  // CRC32 of all payload bytes
};
#pragma pack(pop)

bool CheckpointManager::save(const std::string& filepath, const CheckpointData& data) {
  CheckpointHeader hdr;
  hdr.checkpoint_seq = data.checkpoint_seq;
  hdr.logical_pages = data.logical_pages;
  hdr.physical_pages = data.physical_pages;
  hdr.total_blocks = data.total_blocks;
  hdr.user_open_block = data.user_open_block;
  hdr.user_next_page = data.user_next_page;
  hdr.gc_open_block = data.gc_open_block;
  hdr.gc_next_page = data.gc_next_page;

  std::vector<std::uint8_t> payload;
  auto append_vec = [&payload](const void* src, std::size_t bytes) {
    const auto* p = static_cast<const std::uint8_t*>(src);
    payload.insert(payload.end(), p, p + bytes);
  };

  append_vec(data.l2p.data(), data.l2p.size() * sizeof(PpaIndex));
  append_vec(data.p2l.data(), data.p2l.size() * sizeof(Lba));
  append_vec(data.page_states.data(), data.page_states.size() * sizeof(std::uint8_t));
  append_vec(data.erase_counts.data(), data.erase_counts.size() * sizeof(std::uint32_t));
  append_vec(data.block_states.data(), data.block_states.size() * sizeof(std::uint8_t));

  hdr.payload_bytes = static_cast<std::uint32_t>(payload.size());
  hdr.checksum = crc32(payload.data(), payload.size());

  std::ofstream fout(filepath, std::ios::binary | std::ios::trunc);
  if (!fout.is_open()) return false;

  fout.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
  fout.write(reinterpret_cast<const char*>(payload.data()), payload.size());
  fout.flush();
  return fout.good();
}

bool CheckpointManager::load(const std::string& filepath, CheckpointData& out_data) {
  std::ifstream fin(filepath, std::ios::binary);
  if (!fin.is_open()) return false;

  CheckpointHeader hdr;
  if (!fin.read(reinterpret_cast<char*>(&hdr), sizeof(hdr))) return false;
  if (hdr.magic != 0x43504E54) return false;

  std::vector<std::uint8_t> payload(hdr.payload_bytes);
  if (!fin.read(reinterpret_cast<char*>(payload.data()), hdr.payload_bytes)) return false;

  std::uint32_t computed_crc = crc32(payload.data(), payload.size());
  if (computed_crc != hdr.checksum) return false;

  out_data.checkpoint_seq = hdr.checkpoint_seq;
  out_data.logical_pages = hdr.logical_pages;
  out_data.physical_pages = hdr.physical_pages;
  out_data.total_blocks = hdr.total_blocks;
  out_data.user_open_block = hdr.user_open_block;
  out_data.user_next_page = hdr.user_next_page;
  out_data.gc_open_block = hdr.gc_open_block;
  out_data.gc_next_page = hdr.gc_next_page;

  std::size_t offset = 0;
  auto read_vec = [&payload, &offset](void* dst, std::size_t bytes) {
    if (offset + bytes > payload.size()) return false;
    std::memcpy(dst, payload.data() + offset, bytes);
    offset += bytes;
    return true;
  };

  out_data.l2p.resize(hdr.logical_pages);
  if (!read_vec(out_data.l2p.data(), hdr.logical_pages * sizeof(PpaIndex))) return false;

  out_data.p2l.resize(hdr.physical_pages);
  if (!read_vec(out_data.p2l.data(), hdr.physical_pages * sizeof(Lba))) return false;

  out_data.page_states.resize(hdr.physical_pages);
  if (!read_vec(out_data.page_states.data(), hdr.physical_pages * sizeof(std::uint8_t))) return false;

  out_data.erase_counts.resize(hdr.total_blocks);
  if (!read_vec(out_data.erase_counts.data(), hdr.total_blocks * sizeof(std::uint32_t))) return false;

  out_data.block_states.resize(hdr.total_blocks);
  if (!read_vec(out_data.block_states.data(), hdr.total_blocks * sizeof(std::uint8_t))) return false;

  return true;
}

}  // namespace ftl
