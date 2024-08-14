#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

#include "Solver2.h"

void AllPerms(size_t n, std::vector<size_t>& cur, std::vector<std::vector<size_t>>& res) {
  if (cur.size() == n) {
    res.push_back(cur);
  }
  for (size_t i = 0; i < n; ++i) {
    if (std::find(cur.begin(), cur.end(), i) != cur.end()) {
      continue;
    }
    cur.push_back(i);
    AllPerms(n, cur, res);
    cur.pop_back();
  }
}

TEST(
  Solver2Test,
  handExample
) {
  const unsigned int inf = std::numeric_limits<unsigned int>::max();
  CostMatrix cost = MakeInitialCostMatrixFromCosts(4, {
    inf, 5, 7, 3,
    2, inf, 1, 9,
    8, 4, inf, 4,
    1, 3, 2, inf
  });
  EXPECT_EQ(LittleTSP(cost), 11);
}

RC_GTEST_PROP(
  Solver2Test,
  RandomExamples,
  ()
) {
  size_t num_stops = 8;
  std::vector<unsigned int> c;
  c.reserve(num_stops * num_stops);
  for (size_t i = 0; i < num_stops; ++i) {
    for (size_t j = 0; j < num_stops; ++j) {
      if (i == j) {
        c.push_back(std::numeric_limits<unsigned int>::max());
      } else {
        c.push_back(*rc::gen::inRange(0, 10));
      }
    }
  }

  CostMatrix cost = MakeInitialCostMatrixFromCosts(num_stops, c);
  unsigned int result = LittleTSP(cost);

  std::vector<std::vector<size_t>> perms;
  std::vector<size_t> scratch;
  AllPerms(num_stops, scratch, perms);

  unsigned int best = std::numeric_limits<unsigned int>::max();
  for (const std::vector<size_t>& perm : perms) {
    unsigned int perm_cost = 0;
    for (size_t i = 0; i < perm.size(); ++i) {
      perm_cost += c[perm[i] * num_stops + perm[(i + 1) % perm.size()]];
    }
    if (perm_cost < best) {
      best = perm_cost;
    }
  }

  RC_ASSERT(result == best);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
