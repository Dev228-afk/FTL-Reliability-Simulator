#pragma once
// Core scalar types shared by every FTL component.

#include <cstdint>
#include <limits>
#include <string>

namespace ftl {

using Lba = std::uint32_t;       // logical page address (one LBA == one flash page in this model)
using PpaIndex = std::uint32_t;  // flat physical page index

constexpr PpaIndex kInvalidPpa = std::numeric_limits<PpaIndex>::max();
constexpr Lba kInvalidLba = std::numeric_limits<Lba>::max();

// FTL-level view of a physical page. NAND itself only knows programmed/erased;
// validity is FTL metadata.
enum class PageState : std::uint8_t { Free = 0, Valid = 1, Invalid = 2 };

// Block lifecycle as seen by the allocator.
enum class BlockState : std::uint8_t { Free = 0, OpenUser = 1, OpenGc = 2, Closed = 3 };

// VULNERABLE: experimental vulnerable implementation. Acknowledges writes
//             before the metadata describing them is durable (checkpoint-only).
// WAL:        fixed implementation. Every mapping change is made durable in a
//             write-ahead log before it is applied and acknowledged.
enum class RecoveryMode : std::uint8_t { Vulnerable = 0, Wal = 1 };

enum class GcPolicy : std::uint8_t { Greedy = 0, CostBenefit = 1 };

// Decoded physical page address.
struct Ppa {
  std::uint32_t channel = 0;
  std::uint32_t die = 0;
  std::uint32_t block = 0;  // block index within the die
  std::uint32_t page = 0;
};

const char* to_string(PageState s);
const char* to_string(BlockState s);
const char* to_string(RecoveryMode m);
const char* to_string(GcPolicy p);
bool parse_recovery_mode(const std::string& text, RecoveryMode& out);
bool parse_gc_policy(const std::string& text, GcPolicy& out);

}  // namespace ftl
