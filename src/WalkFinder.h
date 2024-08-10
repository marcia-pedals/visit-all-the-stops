#pragma once

#include <vector>

#include "cereal/types/vector.hpp"

struct AdjacencyList {
  // edges[i] are the indices of the stops that can be reached from stop i.
  std::vector<std::vector<size_t>> edges;

  template<class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(edges));
  }
};

template <typename WalkVisitor, size_t MaxStops>
void FindAllMinimalWalksDFS(
  WalkVisitor& visitor,
  const AdjacencyList& adjacency_list,
  size_t start,
  std::bitset<MaxStops> target_stops
);

struct CollectorWalkVisitor {
  std::vector<std::vector<size_t>> walks;
  std::vector<size_t> current;

  // If this returns false, this branch of the DFS is pruned.
  // The DFS will always pop the stop, even if this returns false.
  bool PushStop(size_t index) {
    current.push_back(index);
    return true;
  }

  void PopStop() {
    current.pop_back();
  }

  void WalkDone() {
    walks.push_back(current);
  }
};

// === Templated Implementations Below ===

namespace
{

template <size_t MaxStops>
struct RouteSearchStateDFS
{
  // visited_at_stop[i] is the set of all visited stations the last time the search visited stop i.
  std::vector<std::bitset<MaxStops>> visited_at_stop;

  // TODO: Explain what this is all about and also think about whether it is correct.
  std::vector<std::bitset<MaxStops>> loop_aborts;
};

template<typename WalkVisitor, size_t MaxStops>
static void FindAllMinimalWalksDFSRec(
  WalkVisitor& visitor,
  const AdjacencyList& adjacency_list,
  const std::bitset<MaxStops> target_stops,
  RouteSearchStateDFS<MaxStops>& state,
  size_t current_stop,
  const std::bitset<MaxStops> current_visited,
  const int level
) {
  if (!visitor.PushStop(current_stop)) {
    visitor.PopStop();
    return;
  }

  if ((target_stops & current_visited) == target_stops) {
    visitor.WalkDone();
    visitor.PopStop();
    return;
  }

  std::vector<std::bitset<MaxStops>> old_loop_aborts = state.loop_aborts;  // TODO: could probably avoid copy
  const std::bitset<MaxStops> old_visited_at_current_stop = state.visited_at_stop[current_stop];
  state.visited_at_stop[current_stop] = current_visited;
  if (old_visited_at_current_stop.any()) {
    state.loop_aborts.push_back(current_visited & (~old_visited_at_current_stop));
  }

  for (std::bitset<MaxStops>& loop_abort : state.loop_aborts) {
    loop_abort[current_stop] = false;
    if ((loop_abort & target_stops).none()) {
        state.visited_at_stop[current_stop] = old_visited_at_current_stop;
        state.loop_aborts = old_loop_aborts;
        visitor.PopStop();
        return;
    }
  }

  int num_processed_next_stops = 0;
  for (const size_t next_stop: adjacency_list.edges[current_stop]) {
    if (level <= 1) {
      for (int i = 0; i < level; ++i) {
        std::cout << "  ";
      }
      std::cout << num_processed_next_stops << " of " << adjacency_list.edges[current_stop].size() << "\n";
    }

    std::bitset<MaxStops> next_visited = current_visited;
    next_visited[next_stop] = true;
    FindAllMinimalWalksDFSRec(visitor, adjacency_list, target_stops, state, next_stop, next_visited, level + 1);
    num_processed_next_stops += 1;
  }

  state.visited_at_stop[current_stop] = old_visited_at_current_stop;
  state.loop_aborts = old_loop_aborts;
  visitor.PopStop();
}

}; // namespace

template <typename WalkVisitor, size_t MaxStops>
void FindAllMinimalWalksDFS(
  WalkVisitor &visitor,
  const AdjacencyList &adjacency_list,
  std::bitset<MaxStops> target_stops
) {
  assert (adjacency_list.edges.size() <= MaxStops);

  size_t processed_starts = 0;
  for (size_t start = 0; start < adjacency_list.edges.size(); ++start) {
    if (!target_stops[start]) {
      continue;
    }
    RouteSearchStateDFS<MaxStops> state;
    state.visited_at_stop.resize(adjacency_list.edges.size());
    std::bitset<MaxStops> start_visited;
    start_visited[start] = true;
    FindAllMinimalWalksDFSRec(visitor, adjacency_list, target_stops, state, start, start_visited, 0);

    processed_starts += 1;
    std::cout << "processed " << processed_starts << " of " << target_stops.count() << "\n";
  }
}
