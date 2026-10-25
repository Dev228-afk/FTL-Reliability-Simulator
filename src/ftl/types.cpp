#include "ftl/types.hpp"

namespace ftl {

const char* to_string(PageState s) {
  switch (s) {
    case PageState::Free: return "FREE";
    case PageState::Valid: return "VALID";
    case PageState::Invalid: return "INVALID";
  }
  return "UNKNOWN";
}

const char* to_string(BlockState s) {
  switch (s) {
    case BlockState::Free: return "FREE";
    case BlockState::OpenUser: return "OPEN_USER";
    case BlockState::OpenGc: return "OPEN_GC";
    case BlockState::Closed: return "CLOSED";
  }
  return "UNKNOWN";
}

const char* to_string(RecoveryMode m) {
  switch (m) {
    case RecoveryMode::Vulnerable: return "vulnerable";
    case RecoveryMode::Wal: return "wal";
  }
  return "unknown";
}

const char* to_string(GcPolicy p) {
  switch (p) {
    case GcPolicy::Greedy: return "greedy";
    case GcPolicy::CostBenefit: return "cost_benefit";
  }
  return "unknown";
}

bool parse_recovery_mode(const std::string& text, RecoveryMode& out) {
  if (text == "vulnerable") { out = RecoveryMode::Vulnerable; return true; }
  if (text == "wal" || text == "fixed") { out = RecoveryMode::Wal; return true; }
  return false;
}

bool parse_gc_policy(const std::string& text, GcPolicy& out) {
  if (text == "greedy") { out = GcPolicy::Greedy; return true; }
  if (text == "cost_benefit" || text == "cost-benefit") { out = GcPolicy::CostBenefit; return true; }
  return false;
}

}  // namespace ftl
