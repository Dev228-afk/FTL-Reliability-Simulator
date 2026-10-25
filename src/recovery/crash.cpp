#include "recovery/crash.hpp"

namespace ftl {

const char* to_string(CrashPoint cp) {
  switch (cp) {
    case CrashPoint::None: return "NONE";
    case CrashPoint::AfterDataProgram: return "AFTER_DATA_PROGRAM";
    case CrashPoint::BeforeMappingUpdate: return "BEFORE_MAPPING_UPDATE";
    case CrashPoint::AfterMappingUpdate: return "AFTER_MAPPING_UPDATE";
    case CrashPoint::AfterAck: return "AFTER_ACK";
    case CrashPoint::DuringCheckpoint: return "DURING_CHECKPOINT";
    case CrashPoint::DuringGcCopy: return "DURING_GC_COPY";
    case CrashPoint::BeforeBlockErase: return "BEFORE_BLOCK_ERASE";
  }
  return "UNKNOWN";
}

bool parse_crash_point(const std::string& text, CrashPoint& out) {
  if (text == "NONE" || text == "none") { out = CrashPoint::None; return true; }
  if (text == "AFTER_DATA_PROGRAM" || text == "after_data_program") { out = CrashPoint::AfterDataProgram; return true; }
  if (text == "BEFORE_MAPPING_UPDATE" || text == "before_mapping_update") { out = CrashPoint::BeforeMappingUpdate; return true; }
  if (text == "AFTER_MAPPING_UPDATE" || text == "after_mapping_update") { out = CrashPoint::AfterMappingUpdate; return true; }
  if (text == "AFTER_ACK" || text == "after_ack") { out = CrashPoint::AfterAck; return true; }
  if (text == "DURING_CHECKPOINT" || text == "during_checkpoint") { out = CrashPoint::DuringCheckpoint; return true; }
  if (text == "DURING_GC_COPY" || text == "during_gc_copy") { out = CrashPoint::DuringGcCopy; return true; }
  if (text == "BEFORE_BLOCK_ERASE" || text == "before_block_erase") { out = CrashPoint::BeforeBlockErase; return true; }
  return false;
}

}  // namespace ftl
