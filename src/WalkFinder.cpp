#include "WalkFinder.h"

#include <assert.h>
#include <bitset>
#include <iostream>

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
static void FindAllRoutesDFSRec(
  WalkVisitor& visitor,
  const AdjacencyList& adjacency_list,
  const std::bitset<MaxStops> target_visited,
  RouteSearchStateDFS<MaxStops>& state,
  size_t current_stop,
  const std::bitset<MaxStops> current_visited
) {
  visitor.PushStop(current_stop);

  if (current_visited == target_visited) {
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
    if (loop_abort.none()) {
        state.visited_at_stop[current_stop] = old_visited_at_current_stop;
        state.loop_aborts = old_loop_aborts;
        visitor.PopStop();
        return;
    }
  }

  for (const size_t next_stop: adjacency_list.edges[current_stop]) {
    std::bitset<MaxStops> next_visited = current_visited;
    next_visited[next_stop] = true;
    FindAllRoutesDFSRec(visitor, adjacency_list, target_visited, state, next_stop, next_visited);
  }

  state.visited_at_stop[current_stop] = old_visited_at_current_stop;
  state.loop_aborts = old_loop_aborts;
  visitor.PopStop();
}

}; // namespace

template <typename WalkVisitor, size_t MaxStops>
void FindAllMinimalWalksDFS(WalkVisitor &visitor, const AdjacencyList &adjacency_list, size_t start) {
  assert (start < adjacency_list.edges.size());

  RouteSearchStateDFS<MaxStops> state;
  state.visited_at_stop.resize(adjacency_list.edges.size());

  assert(adjacency_list.edges.size() <= MaxStops);

  std::bitset<MaxStops> target_visited;
  for (size_t i = 0; i < adjacency_list.edges.size(); i++)
  {
    target_visited[i] = true;
  }

  std::bitset<MaxStops> start_visited;
  start_visited[start] = true;

  FindAllRoutesDFSRec(visitor, adjacency_list, target_visited, state, start, start_visited);
}

template void FindAllMinimalWalksDFS<CollectorWalkVisitor, 32>(
  CollectorWalkVisitor &visitor,
  const AdjacencyList &adjacency_list,
  size_t start
);
