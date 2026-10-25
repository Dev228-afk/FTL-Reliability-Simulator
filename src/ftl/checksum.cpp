#include "ftl/checksum.hpp"

#include <array>
#include <cstdio>

namespace ftl {
namespace {

std::array<std::uint32_t, 256> make_table() {
  std::array<std::uint32_t, 256> table{};
  for (std::uint32_t i = 0; i < 256; ++i) {
    std::uint32_t c = i;
    for (int k = 0; k < 8; ++k) {
      c = (c & 1U) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
    }
    table[i] = c;
  }
  return table;
}

const std::array<std::uint32_t, 256>& table() {
  static const std::array<std::uint32_t, 256> t = make_table();
  return t;
}

}  // namespace

std::uint32_t crc32(const void* data, std::size_t len, std::uint32_t seed) {
  const auto* p = static_cast<const std::uint8_t*>(data);
  const auto& t = table();
  std::uint32_t c = seed ^ 0xFFFFFFFFU;
  for (std::size_t i = 0; i < len; ++i) {
    c = t[(c ^ p[i]) & 0xFFU] ^ (c >> 8);
  }
  return c ^ 0xFFFFFFFFU;
}

std::string crc32_hex(std::uint32_t value) {
  char buf[9];
  std::snprintf(buf, sizeof(buf), "%08x", static_cast<unsigned>(value));
  return std::string(buf, 8);
}

bool parse_crc32_hex(const std::string& text, std::uint32_t& out) {
  if (text.empty() || text.size() > 8) return false;
  std::uint32_t v = 0;
  for (char ch : text) {
    v <<= 4;
    if (ch >= '0' && ch <= '9') v |= static_cast<std::uint32_t>(ch - '0');
    else if (ch >= 'a' && ch <= 'f') v |= static_cast<std::uint32_t>(ch - 'a' + 10);
    else if (ch >= 'A' && ch <= 'F') v |= static_cast<std::uint32_t>(ch - 'A' + 10);
    else return false;
  }
  out = v;
  return true;
}

}  // namespace ftl
