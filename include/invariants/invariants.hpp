#pragma once
// Invariant Verification Engine.
// Enforces mathematical guarantees of the FTL and flags any corruption immediately:
// 1. Mapping Consistency: Mapped LBA must point to a VALID physical page.
// 2. Reverse Mapping Agreement: L2P[LBA] == PPA <=> P2L[PPA] == LBA.
// 3. Unique Ownership: No physical page is owned by more than one active LBA.
// 4. Free Page Integrity: FREE pages cannot be referenced by any LBA.
// 5. Block Accounting: Valid + Invalid + Free == PagesPerBlock for all blocks.
// 6. Durability Check: Every acknowledged write must match its expected data payload and CRC.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "ftl/types.hpp"

namespace ftl {

class Ftl;  // forward declaration

struct InvariantViolation {
  std::string name;
  std::string details;
};

struct InvariantReport {
  bool passed = true;
  std::vector<InvariantViolation> violations;

  void add(const std::string& name, const std::string& details) {
    passed = false;
    violations.push_back({name, details});
  }
};

class InvariantChecker {
 public:
  // Structural checks on internal tables
  static InvariantReport check_structural(const Ftl& ftl);

  // Verifies that all host-acknowledged writes are preserved and return matching data
  static InvariantReport check_acknowledged_writes(
      Ftl& ftl,
      const std::map<Lba, std::pair<std::uint32_t, std::vector<std::uint8_t>>>& acked_writes);
};

}  // namespace ftl
