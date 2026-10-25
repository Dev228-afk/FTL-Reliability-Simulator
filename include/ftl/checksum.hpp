#pragma once
// CRC32 (IEEE 802.3, reflected, poly 0xEDB88320). Identical to zlib.crc32 so
// Python trace generators and the C++ core agree on write checksums.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ftl {

std::uint32_t crc32(const void* data, std::size_t len, std::uint32_t seed = 0);
inline std::uint32_t crc32(const std::string& s) { return crc32(s.data(), s.size()); }
inline std::uint32_t crc32(const std::vector<std::uint8_t>& v) { return crc32(v.data(), v.size()); }

std::string crc32_hex(std::uint32_t value);           // 8 lowercase hex digits
bool parse_crc32_hex(const std::string& text, std::uint32_t& out);

}  // namespace ftl
