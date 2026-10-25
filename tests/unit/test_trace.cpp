#include <gtest/gtest.h>

#include "test_helpers.hpp"
#include "workload/trace.hpp"

namespace ftl {

TEST(TraceTest, SaveAndLoadTrace) {
  testing::TempDir temp_dir;
  std::string trace_path = temp_dir.path() + "/test_trace.jsonl";

  std::vector<TraceRecord> records;
  TraceRecord r1;
  r1.seq = 1;
  r1.op = TraceOp::Write;
  r1.lba = 10;
  r1.data.assign(4096, 0xAB);
  r1.crc = 0x1234;

  TraceRecord r2;
  r2.seq = 2;
  r2.op = TraceOp::Read;
  r2.lba = 10;

  TraceRecord r3;
  r3.seq = 3;
  r3.op = TraceOp::Trim;
  r3.lba = 10;

  records.push_back(r1);
  records.push_back(r2);
  records.push_back(r3);

  TraceParser::save_to_file(trace_path, records);
  auto loaded = TraceParser::load_from_file(trace_path);

  ASSERT_EQ(loaded.size(), 3U);
  EXPECT_EQ(loaded[0].seq, 1U);
  EXPECT_EQ(loaded[0].op, TraceOp::Write);
  EXPECT_EQ(loaded[0].lba, 10U);

  EXPECT_EQ(loaded[1].seq, 2U);
  EXPECT_EQ(loaded[1].op, TraceOp::Read);

  EXPECT_EQ(loaded[2].seq, 3U);
  EXPECT_EQ(loaded[2].op, TraceOp::Trim);
}

}  // namespace ftl
