#pragma once

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ftl/config.hpp"
#include "ftl/types.hpp"

namespace ftl::testing {

// Provides a lightweight test configuration suitable for fast unit/integration testing
SimConfig make_test_config(RecoveryMode mode = RecoveryMode::Wal, std::uint32_t blocks = 8, std::uint32_t ppb = 16);

// Creates a temporary isolated directory for persistence files and removes it on destruction
class TempDir {
 public:
  TempDir();
  ~TempDir();

  const std::string& path() const { return path_; }

 private:
  std::string path_;
};

}  // namespace ftl::testing
