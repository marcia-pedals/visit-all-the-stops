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

  // Check that it's actually a walk. (It hits all the target stops).
  for (size_t i = 0; i < num_stops; ++i) {
    if (target_stops[i] && visit_count[i] == 0) {
      return false;
    }
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


TEST(
  WalkFinderText,
  minimalWalkPropertyImplementedCorrectly
) {
  // This is just testing the property for the next test.

  EXPECT_TRUE(isMinimalWalk({0, 1, 2, 3}, 4, std::bitset<4>("1111")));
  EXPECT_FALSE(isMinimalWalk({0, 1, 0, 1, 2}, 3, std::bitset<4>("1111")));
  EXPECT_TRUE(isMinimalWalk({0, 1, 3, 0, 2}, 4, std::bitset<4>("1111")));

  // "1" is not a target stop, so you can remove the whole loop from 0 to 0.
  EXPECT_FALSE(isMinimalWalk({0, 1, 0, 2, 3}, 4, std::bitset<4>("1101")));

  // Doesn't hit all the target stops.
  EXPECT_FALSE(isMinimalWalk({0, 1, 0, 2}, 4, std::bitset<4>("1101")));
}

RC_GTEST_PROP(
  WalkFinderTest,
  resultsAreMinimalWalks,
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
  example1_notAllStops1
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

  CollectorWalkVisitor visitor;
  FindAllMinimalWalksDFS<CollectorWalkVisitor, 32>(visitor, adjacency_list, 0, target_stops);
  EXPECT_THAT(visitor.walks, ::testing::UnorderedElementsAre(
    ::testing::ElementsAre(0, 2, 3),

    // The following one really is minimal, even though it's pretty counterintuitive. (Imagine if
    // the 2->3 edge is really really slow. Then the following one is the best walk!)
    ::testing::ElementsAre(0, 1, 3, 0, 2)
  ));
}

TEST(
  WalkFinderTest,
  example1_notAllStops2
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
  std::bitset<32> target_stops("0011");

  CollectorWalkVisitor visitor;
  FindAllMinimalWalksDFS<CollectorWalkVisitor, 32>(visitor, adjacency_list, 0, target_stops);
  EXPECT_THAT(visitor.walks, ::testing::UnorderedElementsAre(
    ::testing::ElementsAre(0, 1)
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
