#pragma once
// Deterministic Crash Injection Engine.
// Simulates sudden power loss or controller abrupt reset at precise points
// in the write, trim, GC, and persistence paths.

#include <cstdint>
#include <exception>
#include <string>

namespace ftl {

enum class CrashPoint : std::uint8_t {
  None = 0,
  AfterDataProgram,       // NAND page programmed, but before WAL or metadata update
  BeforeMappingUpdate,    // NAND programmed and WAL appended, but before memory L2P update
  AfterMappingUpdate,     // memory L2P updated, before ack
  AfterAck,               // acknowledged to host, before periodic checkpoint
  DuringCheckpoint,       // partial/corrupt checkpoint write
  DuringGcCopy,           // GC valid page copied, before mapping update
  BeforeBlockErase        // GC copy complete, before victim block erased
};

const char* to_string(CrashPoint cp);
bool parse_crash_point(const std::string& text, CrashPoint& out);

struct CrashTrigger {
  bool enabled = false;
  std::uint32_t trigger_seq = 0;
  CrashPoint point = CrashPoint::None;
};

class CrashException : public std::exception {
 public:
  CrashException(CrashPoint point, std::uint32_t seq)
      : point_(point), seq_(seq), msg_("Injected Crash at " + std::string(to_string(point)) + " on seq " + std::to_string(seq)) {}

  const char* what() const noexcept override { return msg_.c_str(); }
  CrashPoint point() const { return point_; }
  std::uint32_t seq() const { return seq_; }

 private:
  CrashPoint point_;
  std::uint32_t seq_;
  std::string msg_;
};

class CrashInjector {
 public:
  explicit CrashInjector(const CrashTrigger& trigger = CrashTrigger{}) : trigger_(trigger) {}

  void set_trigger(const CrashTrigger& trigger) { trigger_ = trigger; }
  const CrashTrigger& trigger() const { return trigger_; }

  void check(CrashPoint point, std::uint32_t seq) {
    if (trigger_.enabled && trigger_.point == point && trigger_.trigger_seq == seq) {
      throw CrashException(point, seq);
    }
  }

 private:
  CrashTrigger trigger_;
};

}  // namespace ftl
