#pragma once

#include <cassert>
#include <compare>
#include <iterator>
#include <ostream>

#include <boost/functional/hash.hpp>
#include <rapidcheck.h>

// <struct Range>

struct Range {
  unsigned int start;
  unsigned int finish;  // inclusive!!!
  unsigned int interval;

  Range(unsigned int start, unsigned int finish, unsigned int interval)
    : start(start), finish(finish), interval(interval) {
      assert(finish >= start);
      assert(finish == start || (interval > 0 && (finish - start) % interval == 0));
    }

  friend auto operator<=>(const Range&, const Range&) = default;

  // <Iterator>

  class Iterator {
  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = unsigned int;
    using difference_type = int;
    using pointer = const unsigned int*;
    using reference = const unsigned int&;

    Iterator(const Range& range, unsigned int index) : range(range), index(index) {}

    bool operator==(const Iterator& other) const {
      return index == other.index;
    }

    bool operator!=(const Iterator& other) const {
      return index != other.index;
    }

    Iterator& operator++() {
      index += 1;
      return *this;
    }

    unsigned int operator*() const {
      return range.start + index * range.interval;
    }

    int operator-(const Iterator& other) const {
      return index - other.index;
    }

  private:
    const Range& range;
    int index;
  };

  Iterator begin() const {
    return Iterator(*this, 0);
  }

  Iterator end() const {
    if (interval == 0) {
      return Iterator(*this, 1);
    }
    return Iterator(*this, (finish - start) / interval + 1);
  }

  // </Iterator>
};

std::ostream &operator<<(std::ostream &os, const Range& range);

namespace std
{
  template <>
  struct hash<Range>
  {
    std::size_t operator()(const Range& range) const noexcept
    {
      std::size_t h = 0;
      boost::hash_combine(h, range.start);
      boost::hash_combine(h, range.finish);
      boost::hash_combine(h, range.interval);
      return h;
    }
  };
}

namespace rc {
  template <>
  struct Arbitrary<Range> {
    static Gen<Range> arbitrary() {
      return gen::mapcat(gen::inRange<unsigned int>(0, 100).as("start"), [](unsigned int start) {
        return gen::mapcat(gen::inRange<unsigned int>(0, 100).as("interval"), [start](unsigned int interval) {
          if (interval == 0) {
            return gen::just(Range(start, start, interval));
          }
          return gen::map(gen::inRange<unsigned int>(0, 20).as("repeats"), [start, interval](unsigned int repeats) {
            return Range(start, start + interval * repeats, interval);
          });
        });
      });
    }
  };
}

// </struct Range>


// A "minimal connection" between ranges a and b is a pair of elements (ai, bj) such that:
// - ai <= bi
// - ai is the biggest element in a that is <= bi
// - bj is the smallest element in b that is >= ai
//
// Returns all minimal connections.
std::vector<std::tuple<unsigned int, unsigned int>> naiveComputeMinimalConnections(const Range& a, const Range& b);
