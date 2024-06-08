#include <algorithm>
#include <iterator>

#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

#include "MultiSegment.h"
#include <gmock/gmock-matchers.h>

RC_GTEST_PROP(
  MultiSegmentTest,
  rangeBeginIsStart,
  (const Range& range)
) {
  RC_ASSERT(*range.begin() == range.start);
}

RC_GTEST_PROP(
  MultiSegmentTest,
  rangeLastIsFinish,
  (const Range& range)
) {
  int last;
  for (const unsigned int x : range) {
    last = x;
  }
  RC_ASSERT(last == range.finish);
}

RC_GTEST_PROP(
  MultiSegmentTest,
  rangeSizeIsCorrect,
  (const Range& range)
) {
  unsigned int expected_size = range.interval == 0 ? 1 : (range.finish - range.start) / range.interval + 1;
  RC_ASSERT(std::distance(range.begin(), range.end()) == expected_size);
}

RC_GTEST_PROP(
  MultiSegmentTest,
  naiveComputeMinimalConnections_oneElement,
  ()
) {
  unsigned int first = *rc::gen::inRange<unsigned int>(0, 100);
  unsigned int second = *rc::gen::inRange<unsigned int>(0, 100);
  Range a(first, first, 0);
  Range b(second, second, 0);
  auto result = naiveComputeMinimalConnections(a, b);
  std::vector<std::tuple<unsigned int, unsigned int>> expected;
  if (first <= second) {
    expected.push_back(std::make_tuple(first, second));
  }
  RC_ASSERT(result == expected);
}

RC_GTEST_PROP(
  MultiSegmentTest,
  naiveComputeMinimalConnections_disjointRanges,
  (const Range& a, const Range& b)
) {
  Range b_disjoint(b.start + 1000000, b.finish + 1000000, b.interval);
  auto result = naiveComputeMinimalConnections(a, b_disjoint);
  std::vector<std::tuple<unsigned int, unsigned int>> expected;
  expected.push_back(std::make_tuple(a.finish, b_disjoint.start));
  RC_ASSERT(result == expected);
}

RC_GTEST_PROP(
  MultiSegmentTest,
  naiveComputeMinimalConnections_uniformlyoffset,
  ()
) {
  Range a = *rc::gen::suchThat<Range>([](const Range& r) { return r.interval > 0; });
  unsigned int offset = *rc::gen::inRange<unsigned int>(0, a.interval);
  Range b = Range(a.start + offset, a.finish + offset, a.interval);
  auto result = naiveComputeMinimalConnections(a, b);
  std::vector<std::tuple<unsigned int, unsigned int>> expected;
  for (const unsigned int ai : a) {
    expected.push_back(std::make_tuple(ai, ai + offset));
  }
  RC_ASSERT(result == expected);
}

TEST(MultiSegmentTest, naiveComputeMinimalConnections_example) {
  Range a(0, 50, 5);
  Range b(10, 80, 7);
  std::vector<std::tuple<unsigned int, unsigned int>> expected;
  expected.push_back(std::make_tuple(10, 10));
  expected.push_back(std::make_tuple(15, 17));
  expected.push_back(std::make_tuple(20, 24));
  expected.push_back(std::make_tuple(30, 31));
  expected.push_back(std::make_tuple(35, 38));
  expected.push_back(std::make_tuple(45, 45));
  expected.push_back(std::make_tuple(50, 52));
  EXPECT_EQ(naiveComputeMinimalConnections(a, b), expected);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
