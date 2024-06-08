#include "MultiSegment.h"
#include <numeric>

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

std::vector<std::tuple<unsigned int, unsigned int>> smarterComputeMinimalConnections(const Range& a, const Range& b) {
  std::vector<std::tuple<unsigned int, unsigned int>> result;
  auto a_it = a.begin();
  auto b_it = b.begin();
  while (a_it != a.end()) {
    while (b_it != b.end() && *b_it < *a_it) {
      ++b_it;
    }
    if (b_it == b.end()) {
      break;
    }
    if (std::next(a_it) == a.end() || *std::next(a_it) > *b_it) {
      result.push_back(std::make_tuple(*a_it, *b_it));
    }
    ++a_it;
  }
  return result;
}

unsigned int ceil_divide(unsigned int a, unsigned int b) {
  return (a + b - 1) / b;
}

static std::optional<std::tuple<unsigned int, unsigned int>> findFirstMinimalConnection(const Range& a, const Range& b) {
  // First we find the element of b in the first minimal connection.
  unsigned int first_minimal_connection_b;
  if (b.finish < a.start) {
    // Oh no, no elements of b are reachable, so there are no connections.
    return std::nullopt;
  }
  if (b.start >= a.start) {
    first_minimal_connection_b = b.start;
  } else {
    // Note: b.interval cannot be 0 here, because if it were, than b.start == b.finish so one of the
    // above conditions would have been true.
    first_minimal_connection_b = ceil_divide(a.start - b.start, b.interval) * b.interval + b.start;
  }

  if (a.interval == 0) {
    return std::make_tuple(a.start, first_minimal_connection_b);
  }

  // Now we find the element of a in the first minimal connection.
  unsigned int first_minimal_connection_a = std::min(
    a.finish,
    (first_minimal_connection_b - a.start) / a.interval * a.interval + a.start
  );

  return std::make_tuple(first_minimal_connection_a, first_minimal_connection_b);
}

static std::optional<Range> withNewStart(const Range& range, unsigned int new_start) {
  if (new_start > range.finish) {
    return std::nullopt;
  }
  if (new_start == range.finish) {
    return Range(new_start, new_start, 0);
  }
  return Range(new_start, range.finish, range.interval);
}

std::vector<std::tuple<Range, unsigned int>> computeMinimalConnections(Range a, Range b) {
  // Throughout this function, "interval" 0 means that there is only a single element, and we have a
  // lot of special case handling for this.

  // We accumulate the result here.
  std::vector<std::tuple<Range, unsigned int>> result;

  // Keep track of whether we used the last element of a, so that we know if we need to add its
  // minimal connection at the end.
  bool used_last_element_of_a = false;

  // Repeats of minimal connections at this interval are minimal.
  // Exception: A repeat of the very first minimal connection might not be minimal.
  unsigned int repeat_interval = (a.interval > 0 && b.interval > 0) ? std::lcm(a.interval, b.interval) : 0;

  // Find the first minimal connection, and remember it, because we check how far past it to decide
  // when to terminate.
  auto first_minimal_connection = findFirstMinimalConnection(a, b);
  if (!first_minimal_connection.has_value()) {
    return {};
  }
  auto [first_ai, first_bj] = *first_minimal_connection;

  // Edge case: Check whether a repeat of this connection is minimal, and if not, add a single
  // connection for this one and then advance first_ai, first_bj. After we have done this, we can
  // assume that all repeats of all connections we find are minimal.
  if (repeat_interval > 0) {
    auto a_updated_opt = withNewStart(a, first_ai + repeat_interval);
    if (a_updated_opt.has_value()) {
      auto repeat_minimal_connection = findFirstMinimalConnection(*a_updated_opt, b);
      if (repeat_minimal_connection.has_value()) {
        auto [ai, bj] = *repeat_minimal_connection;
        if (ai != first_ai + repeat_interval || bj != first_bj + repeat_interval) {
          result.push_back(std::make_tuple(Range(first_ai, first_ai, 0), first_bj - first_ai));
          used_last_element_of_a |= (first_ai == a.finish);

          auto new_a = withNewStart(a, first_ai + a.interval);
          assert(new_a.has_value());
          a = *new_a;

          first_minimal_connection = findFirstMinimalConnection(a, b);
          assert(first_minimal_connection.has_value());
          first_ai = std::get<0>(*first_minimal_connection);
          first_bj = std::get<1>(*first_minimal_connection);
        }
      }
    }
  }

  // Now advance a one minimal connection at a time until we've cycled through exactly one repeat_interval.
  for (;;) {
    auto next_minimal_connection = findFirstMinimalConnection(a, b);
    if (!next_minimal_connection.has_value()) {
      break;
    }
    auto [ai, bj] = *next_minimal_connection;
    if (repeat_interval > 0 && ai - first_ai >= repeat_interval) {
      // We have done one full cycle, so we're done: the rest of the connections are covered by
      // repeats of the ranges we have already found.

      // Assert that we hit exactly one repeat interval because everything should be repeating
      // precisely at the repeat interval.
      assert(ai - first_ai == repeat_interval && bj - first_bj == repeat_interval);

      break;
    }

    // Put all possible repeat_interval repeats of this connection into the result.
    unsigned int num_repeats = repeat_interval == 0 ? 0 : std::min(
      (a.finish - ai) / repeat_interval,
      (b.finish - bj) / repeat_interval
    );
    unsigned int finish = ai + num_repeats * repeat_interval;
    result.push_back(std::make_tuple(Range(ai, finish, num_repeats == 0 ? 0 : repeat_interval), bj - ai));
    used_last_element_of_a |= (finish == a.finish);

    // Special case: If either interval is 0, then the range is a single-element so we've found all
    // connections.
    if (a.interval == 0 || b.interval == 0) {
      break;
    }

    // Move the start of a past this minimal connection so that next time we find a new minimal connection.
    auto new_a = withNewStart(a, ai + a.interval);
    if (!new_a.has_value()) {
      break;
    }
    a = *new_a;
  }

  // The last element of a might be part of a minimal connection that the loop did not find.
  if (!used_last_element_of_a) {
    auto last_element_of_a_minimal_connection = findFirstMinimalConnection(Range(a.finish, a.finish, 0), b);
    if (last_element_of_a_minimal_connection.has_value()) {
      auto [ai, bj] = *last_element_of_a_minimal_connection;
      result.push_back(std::make_tuple(Range(ai, ai, 0), bj - ai));
      used_last_element_of_a = true;
    }
  }

  return result;
}
