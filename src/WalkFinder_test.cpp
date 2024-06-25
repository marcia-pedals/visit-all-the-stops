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

bool isMinimalWalk(const std::vector<size_t>& walk, size_t num_stops) {
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

      // See if there's a stop that we visited in the excursion that we didn't visit before or after.
      bool found_necessary_stop = false;
      for (size_t candidate = 0; candidate < num_stops; ++candidate) {
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
  std::unordered_set<size_t> expected;
  for (size_t i = 0; i < adjacency_list.edges.size(); ++i) {
    expected.insert(i);
  }

  CollectorWalkVisitor visitor;
  FindAllMinimalWalksDFS<CollectorWalkVisitor, 32>(visitor, adjacency_list, 0);
  for (const auto& walk : visitor.walks) {
    std::unordered_set<size_t> actual(walk.begin(), walk.end());
    EXPECT_EQ(expected, actual);
  }
}

TEST(
  WalkFinderText,
  minimalPropertyImplementedCorrectly
) {
  // This is just testing the property for the next test.
  EXPECT_TRUE(isMinimalWalk({0, 1, 2, 3}, 4));
  EXPECT_FALSE(isMinimalWalk({0, 1, 0, 1, 2}, 3));
}

RC_GTEST_PROP(
  WalkFinderTest,
  walksAreMinimal,
  ()
) {
  AdjacencyList adjacency_list = arbitraryAdjacencyList();
  CollectorWalkVisitor visitor;
  FindAllMinimalWalksDFS<CollectorWalkVisitor, 32>(visitor, adjacency_list, 0);
  for (const auto& walk : visitor.walks) {
    EXPECT_TRUE(isMinimalWalk(walk, adjacency_list.edges.size()));
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

  CollectorWalkVisitor visitor;
  FindAllMinimalWalksDFS<CollectorWalkVisitor, 32>(visitor, adjacency_list, 0);
  EXPECT_THAT(visitor.walks, ::testing::UnorderedElementsAre(
    ::testing::ElementsAre(0, 1, 3, 0, 2),
    ::testing::ElementsAre(0, 2, 3, 0, 1)
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

  CollectorWalkVisitor visitor;
  FindAllMinimalWalksDFS<CollectorWalkVisitor, 32>(visitor, adjacency_list, 0);
  EXPECT_THAT(visitor.walks, ::testing::UnorderedElementsAre(
    ::testing::ElementsAre(0, 1, 2, 3)
  ));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
