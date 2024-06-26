#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

#include "WalkFinder.h"

namespace {

AdjacencyList arbitraryAdjacencyList() {
  AdjacencyList result;
  const size_t num_stops = *rc::gen::inRange<size_t>(1, 9);
  for (size_t origin = 0; origin < num_stops; ++origin) {
    result.edges.push_back({});
    for (size_t destination = 0; destination < num_stops; ++destination) {
      if (*rc::gen::arbitrary<bool>()) {
        result.edges.back().push_back(destination);
      }
    }
  }
  return result;
}

template <size_t MaxStops>
bool isMinimalWalk(const std::vector<size_t>& walk, size_t num_stops, std::bitset<MaxStops> target_stops) {
  // The "current" cumulative number of times each stop has been visited.
  std::vector<size_t> visit_count(num_stops, 0);

  // A scan of `visit_count` after each stop in the walk.
  std::vector<std::vector<size_t>> visit_count_history;

  // visit_count_at_stop[i] is all the states of `visit_count` after visiting stop i.
  std::vector<std::vector<std::vector<size_t>>> visit_count_at_stop;
  visit_count_at_stop.resize(num_stops);

  // Fill in all the above vectors.
  for (const size_t stop : walk) {
    visit_count[stop]++;
    visit_count_history.push_back(visit_count);
    visit_count_at_stop[stop].push_back(visit_count);
  }

  // A walk is minimal iff every time you get back to a stop you've visited before, you can't delete
  // that whole excursion and still have visited all the stops. So let's check every case where we
  // get back to a stop we've visited before.
  for (const size_t stop : walk) {
    for (int i = 0; i < visit_count_at_stop[stop].size() - 1; ++i) {
      const auto& visit_count_start = visit_count_at_stop[stop][i];
      const auto& visit_count_end = visit_count_at_stop[stop][i + 1];

      // See if there's a target stop that we visited in the excursion that we didn't visit before or after.
      bool found_necessary_stop = false;
      for (size_t candidate = 0; candidate < num_stops; ++candidate) {
        if (!target_stops[candidate]) {
          continue;
        }
        if (visit_count_start[candidate] == 0 && visit_count_end[candidate] == visit_count[candidate]) {
          found_necessary_stop = true;
          break;
        }
      }
      if (!found_necessary_stop) {
        return false;
      }
    }
  }
  return true;
}
  
}  // namespace


RC_GTEST_PROP(
  WalkFinderTest,
  walksVisitAllStops,
  ()
) {
  AdjacencyList adjacency_list = arbitraryAdjacencyList();

  std::bitset<32> target_stops;
  for (size_t i = 0; i < adjacency_list.edges.size(); ++i) {
    target_stops[i] = *rc::gen::arbitrary<bool>();
  }

  std::unordered_set<size_t> expected;
  for (size_t i = 0; i < adjacency_list.edges.size(); ++i) {
    if (target_stops[i]) {
      expected.insert(i);
    }
  }

  // TODO NOW: When there are no target stops, the walk finder still returns a walk starting at 0, so this test fails.
  // Think more about this edge case and whether I should adjust the test or change the interface of the walk finder so that
  // this isn't an edge case. Maybe it should not take a starting point and instead return all the minimal walks??!
  // Also this seems to suggest that the minimality finder should be able to delete a single vertex walk.

  CollectorWalkVisitor visitor;
  FindAllMinimalWalksDFS<CollectorWalkVisitor, 32>(visitor, adjacency_list, 0, target_stops);
  for (const auto& walk : visitor.walks) {
    std::unordered_set<size_t> actual(walk.begin(), walk.end());
    RC_ASSERT(expected == actual);
  }
}

TEST(
  WalkFinderText,
  minimalPropertyImplementedCorrectly
) {
  // This is just testing the property for the next test.

  EXPECT_TRUE(isMinimalWalk({0, 1, 2, 3}, 4, std::bitset<4>("1111")));
  EXPECT_FALSE(isMinimalWalk({0, 1, 0, 1, 2}, 3, std::bitset<4>("1111")));

  EXPECT_TRUE(isMinimalWalk({0, 1, 3, 0, 2}, 4, std::bitset<4>("1111")));

  // "1" is not a target stop, so you can remove the whole loop from 0 to 0.
  // TODO NOW: Is this failing because `isMinimalWalk` is wrong or because 1101 isn't right for making "1" not a target stop?
  // Probably the former because I tried 1011 and it still fails.
  EXPECT_FALSE(isMinimalWalk({0, 1, 3, 0, 2}, 4, std::bitset<4>("1101")));
}

RC_GTEST_PROP(
  WalkFinderTest,
  walksAreMinimal,
  ()
) {
  AdjacencyList adjacency_list = arbitraryAdjacencyList();

  std::bitset<32> target_stops;
  for (size_t i = 0; i < adjacency_list.edges.size(); ++i) {
    target_stops[i] = *rc::gen::arbitrary<bool>();
  }

  CollectorWalkVisitor visitor;
  FindAllMinimalWalksDFS<CollectorWalkVisitor, 32>(visitor, adjacency_list, 0, target_stops);
  for (const auto& walk : visitor.walks) {
    RC_ASSERT(isMinimalWalk(walk, adjacency_list.edges.size(), target_stops));
  }
}

// TODO: Property test that I find all the minimal walks?
// Maybe have a complete brute force implementation and check that the results are the same?

TEST(
  WalkFinderTest,
  example1
) {
  //   0 <----
  //  / \    |
  // v   v   |
  // 1   2   |
  //  \ /    |
  //   v     |
  //   3------
  AdjacencyList adjacency_list;
  adjacency_list.edges = {
    {1, 2},
    {3},
    {3},
    {0}
  };
  std::bitset<32> target_stops("1111");

  CollectorWalkVisitor visitor;
  FindAllMinimalWalksDFS<CollectorWalkVisitor, 32>(visitor, adjacency_list, 0, target_stops);
  EXPECT_THAT(visitor.walks, ::testing::UnorderedElementsAre(
    ::testing::ElementsAre(0, 1, 3, 0, 2),
    ::testing::ElementsAre(0, 2, 3, 0, 1)
  ));
}

TEST(
  WalkFinderTest,
  example1_notAllStops
) {
  //   0 <----
  //  / \    |
  // v   v   |
  // 1   2   |
  //  \ /    |
  //   v     |
  //   3------
  AdjacencyList adjacency_list;
  adjacency_list.edges = {
    {1, 2},
    {3},
    {3},
    {0}
  };
  std::bitset<32> target_stops("1101");

  // TODO NOW: Hmm, this test is revealing that the minimal property doesn't work correctly
  // when the target stops are a subset of all the stops! It seems to be checking for minimality
  // hitting _all_ the stops.

  CollectorWalkVisitor visitor;
  FindAllMinimalWalksDFS<CollectorWalkVisitor, 32>(visitor, adjacency_list, 0, target_stops);
  EXPECT_THAT(visitor.walks, ::testing::UnorderedElementsAre(
    ::testing::ElementsAre(0, 2, 3)
  ));
}

TEST(
  WalkFinderTest,
  example2
) {
  // 0 --> 1 --> 2 --> 3
  AdjacencyList adjacency_list;
  adjacency_list.edges = {
    {1},
    {2},
    {3},
    {}
  };
  std::bitset<32> target_stops("1111");

  CollectorWalkVisitor visitor;
  FindAllMinimalWalksDFS<CollectorWalkVisitor, 32>(visitor, adjacency_list, 0, target_stops);
  EXPECT_THAT(visitor.walks, ::testing::UnorderedElementsAre(
    ::testing::ElementsAre(0, 1, 2, 3)
  ));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
