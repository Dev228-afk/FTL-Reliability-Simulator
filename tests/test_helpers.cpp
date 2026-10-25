#include "test_helpers.hpp"

#include <chrono>
#include <filesystem>

namespace ftl::testing {

SimConfig make_test_config(RecoveryMode mode, std::uint32_t blocks, std::uint32_t ppb) {
  SimConfig cfg;
  cfg.geometry.channels = 1;
  cfg.geometry.dies_per_channel = 1;
  cfg.geometry.blocks_per_die = blocks;
  cfg.geometry.pages_per_block = ppb;
  cfg.geometry.page_size_bytes = 4096;
  cfg.geometry.overprovision_percent = 25.0;
  cfg.min_spare_blocks = 2;

  cfg.timing.read_us = 10;
  cfg.timing.program_us = 100;
  cfg.timing.erase_us = 500;
  cfg.timing.jitter_us = 0;

  cfg.gc.low_watermark_blocks = 2;
  cfg.gc.policy = GcPolicy::Greedy;

  cfg.wear.dynamic_enabled = true;
  cfg.wear.static_enabled = true;
  cfg.wear.threshold = 4;
  cfg.wear.check_interval_erases = 4;

  cfg.persistence.mode = mode;
  cfg.persistence.checkpoint_interval_ops = 50;
  cfg.persistence.checkpoint_on_flush = true;

  cfg.debug.check_every_op = true;
  cfg.debug.check_after_gc = true;
  cfg.debug.check_after_checkpoint = true;

  cfg.seed = 12345;
  cfg.validate();
  return cfg;
}

TempDir::TempDir() {
  auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
  path_ = "./test_run_" + std::to_string(now);
  std::filesystem::create_directories(path_);
}

TempDir::~TempDir() {
  std::error_code ec;
  std::filesystem::remove_all(path_, ec);
}

}  // namespace ftl::testing
