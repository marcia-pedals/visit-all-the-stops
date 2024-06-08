#include "MultiSegment.h"

// <struct Range>

std::ostream& operator<<(std::ostream& os, const Range& range) {
  return os << "Range(" << range.start << ", " << range.finish << ", " << range.interval << ")";
}

// </struct Range>

std::vector<std::tuple<unsigned int, unsigned int>> naiveComputeMinimalConnections(const Range& a, const Range& b) {
  std::vector<std::tuple<unsigned int, unsigned int>> result;
  for (const unsigned int ai : a) {
    auto b_it = std::find_if(b.begin(), b.end(), [ai](unsigned int bj) { return bj >= ai; });
    if (b_it == b.end()) {
      continue;
    }

    // Okay, we found the smallest bj >= ai.
    unsigned int bj = *b_it;

    // Now we need to check that ai is the largest element in a that is <= bj.
    int max_ai = -1;
    for (const unsigned int ai2 : a) {
      if (ai2 <= bj) {
        max_ai = ai2;
      }
    }
    if (max_ai != ai) {
      continue;
    }
    result.push_back(std::make_tuple(ai, bj));
  }
  return result;
}
