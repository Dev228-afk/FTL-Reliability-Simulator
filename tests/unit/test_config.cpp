#include <gtest/gtest.h>

#include "ftl/config.hpp"

namespace ftl {

TEST(ConfigTest, LogicalPagesAndOverprovisioning) {
  SimConfig cfg;
  cfg.geometry.channels = 1;
  cfg.geometry.dies_per_channel = 1;
  cfg.geometry.blocks_per_die = 100;
  cfg.geometry.pages_per_block = 100;
  cfg.geometry.page_size_bytes = 4096;
  cfg.geometry.overprovision_percent = 25.0;  // 25% OP
  cfg.min_spare_blocks = 4;
  cfg.validate();

  // Total pages = 10,000. 25% OP means user = 10000 / 1.25 = 8000 pages.
  EXPECT_EQ(cfg.geometry.total_pages(), 10000U);
  EXPECT_EQ(cfg.logical_pages(), 8000U);
  EXPECT_DOUBLE_EQ(cfg.effective_op_percent(), 25.0);
}

TEST(ConfigTest, JsonRoundTrip) {
  SimConfig cfg;
  cfg.geometry.channels = 2;
  cfg.geometry.dies_per_channel = 2;
  cfg.geometry.blocks_per_die = 64;
  cfg.geometry.pages_per_block = 128;
  cfg.validate();

  nlohmann::json j = config_to_json(cfg);
  SimConfig restored = config_from_json(j);

  EXPECT_EQ(restored.geometry.channels, 2U);
  EXPECT_EQ(restored.geometry.dies_per_channel, 2U);
  EXPECT_EQ(restored.geometry.blocks_per_die, 64U);
  EXPECT_EQ(restored.geometry.pages_per_block, 128U);
}

TEST(ConfigTest, SimpleYamlParsing) {
  std::string yaml_str = R"(
# Sample comment
geometry:
  channels: 2
  blocks_per_die: 64
gc:
  policy: greedy
seed: 999
)";

  nlohmann::json j = parse_simple_yaml(yaml_str);
  EXPECT_EQ(j["geometry"]["channels"], 2);
  EXPECT_EQ(j["geometry"]["blocks_per_die"], 64);
  EXPECT_EQ(j["gc"]["policy"], "greedy");
  EXPECT_EQ(j["seed"], 999);
}

TEST(ConfigTest, ValidationRejectsInvalidValues) {
  SimConfig cfg;
  cfg.geometry.channels = 0;
  EXPECT_THROW(cfg.validate(), std::invalid_argument);

  cfg.geometry.channels = 1;
  cfg.geometry.blocks_per_die = 2;  // Below minimum 4
  EXPECT_THROW(cfg.validate(), std::invalid_argument);
}

}  // namespace ftl
