#include "invariants/invariants.hpp"

#include <sstream>

#include "ftl/checksum.hpp"
#include "ftl/ftl.hpp"

namespace ftl {

InvariantReport InvariantChecker::check_structural(const Ftl& ftl) {
  InvariantReport rep;
  const auto& l2p = ftl.l2p();
  const auto& p2l = ftl.p2l();
  const auto& page_states = ftl.page_states();
  const auto& block_states = ftl.block_states();
  const auto& geom = ftl.config().geometry;
  const auto ppb = geom.pages_per_block;
  const auto total_blocks = geom.total_blocks();
  const auto total_pages = geom.total_pages();

  // 1. Mapping Consistency & Reverse Mapping Agreement
  for (std::size_t lba = 0; lba < l2p.size(); ++lba) {
    PpaIndex ppa = l2p[lba];
    if (ppa != kInvalidPpa) {
      if (ppa >= total_pages) {
        rep.add("MappingOutOfBounds", "LBA " + std::to_string(lba) + " maps to out-of-bounds PPA " + std::to_string(ppa));
        continue;
      }
      if (page_states[ppa] != PageState::Valid) {
        rep.add("MappedPageNotValid", "LBA " + std::to_string(lba) + " maps to PPA " + std::to_string(ppa) + " which has state " + to_string(page_states[ppa]));
      }
      if (p2l[ppa] != static_cast<Lba>(lba)) {
        rep.add("ReverseMapMismatch", "L2P[" + std::to_string(lba) + "] = " + std::to_string(ppa) + ", but P2L[" + std::to_string(ppa) + "] = " + std::to_string(p2l[ppa]));
      }
    }
  }

  // 2. Reverse Mapping Consistency & Unique Ownership
  std::vector<bool> ppa_owned(total_pages, false);
  for (std::size_t lba = 0; lba < l2p.size(); ++lba) {
    PpaIndex ppa = l2p[lba];
    if (ppa != kInvalidPpa && ppa < total_pages) {
      if (ppa_owned[ppa]) {
        rep.add("DuplicateOwnership", "PPA " + std::to_string(ppa) + " is owned by multiple LBAs (including LBA " + std::to_string(lba) + ")");
      }
      ppa_owned[ppa] = true;
    }
  }

  // 3. Free Page Integrity: FREE pages cannot be mapped or have valid P2L
  for (std::size_t ppa = 0; ppa < total_pages; ++ppa) {
    if (page_states[ppa] == PageState::Free) {
      if (p2l[ppa] != kInvalidLba) {
        rep.add("FreePageHasP2LOwner", "PPA " + std::to_string(ppa) + " is FREE but P2L is " + std::to_string(p2l[ppa]));
      }
    }
  }

  // 4. Block Accounting: Free + Valid + Invalid == pages_per_block
  for (std::uint32_t b = 0; b < total_blocks; ++b) {
    std::uint32_t v_cnt = ftl.valid_pages(b);
    std::uint32_t iv_cnt = ftl.invalid_pages(b);
    std::uint32_t f_cnt = ftl.free_pages(b);

    if (v_cnt + iv_cnt + f_cnt != ppb) {
      std::ostringstream ss;
      ss << "Block " << b << " accounting sum (" << v_cnt << " V + " << iv_cnt << " IV + " << f_cnt << " F) != " << ppb;
      rep.add("BlockAccountingMismatch", ss.str());
    }

    // Verify physical page states in block match the counters
    std::uint32_t actual_v = 0, actual_iv = 0, actual_f = 0;
    const PpaIndex start_ppa = b * ppb;
    for (std::uint32_t p = 0; p < ppb; ++p) {
      PageState st = page_states[start_ppa + p];
      if (st == PageState::Valid) actual_v++;
      else if (st == PageState::Invalid) actual_iv++;
      else if (st == PageState::Free) actual_f++;
    }
    if (actual_v != v_cnt || actual_iv != iv_cnt || actual_f != f_cnt) {
      std::ostringstream ss;
      ss << "Block " << b << " state counters differ from actual page states";
      rep.add("BlockStateCounterDiscrepancy", ss.str());
    }

    if (block_states[b] == BlockState::Free && (actual_v > 0 || actual_iv > 0)) {
      rep.add("FreeBlockContainsProgrammedPages", "Block " + std::to_string(b) + " is marked Free but contains non-free pages");
    }
  }

  return rep;
}

InvariantReport InvariantChecker::check_acknowledged_writes(
    Ftl& ftl,
    const std::map<Lba, std::pair<std::uint32_t, std::vector<std::uint8_t>>>& acked_writes) {
  InvariantReport rep;

  for (const auto& [lba, entry] : acked_writes) {
    const auto& expected_data = entry.second;
    const std::uint32_t expected_seq = entry.first;

    if (lba >= ftl.l2p().size() || ftl.l2p()[lba] == kInvalidPpa) {
      rep.add("AcknowledgedWriteLostUnmapped",
              "Acknowledged write for LBA " + std::to_string(lba) + " was lost (unmapped after recovery, seq " + std::to_string(expected_seq) + ")");
      continue;
    }

    std::vector<std::uint8_t> read_data;
    bool ok = ftl.read(lba, read_data, expected_seq);
    if (!ok) {
      rep.add("ReadFailed", "Failed to read acknowledged LBA " + std::to_string(lba));
      continue;
    }
    if (read_data != expected_data) {
      rep.add("DataCorruptionOrStaleRead",
              "Acknowledged write for LBA " + std::to_string(lba) + " returned corrupt or stale data (seq " + std::to_string(expected_seq) + ")");
    }
  }

  return rep;
}

}  // namespace ftl
