#include "Solver.h"

#include <iostream>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_join.h"

struct Segment {
  WorldTime departure_time;
  WorldTime arrival_time;
};

struct GroupedSegments {
  size_t destination_stop_index;
  std::vector<Segment> segments;
};

struct Problem {
  // Mappings between World and Problem ids/indices.
  absl::flat_hash_map<std::string, size_t> stop_id_to_index;
  std::vector<std::string> stop_index_to_id;

  // segments[i] are all the segments originating from stop (index) i.
  std::vector<std::vector<GroupedSegments>> segments;
};

static size_t GetOrAddStop(const std::string& stop_id, Problem& problem) {
  if (problem.stop_id_to_index.contains(stop_id)) {
    return problem.stop_id_to_index.at(stop_id);
  }
  problem.stop_id_to_index[stop_id] = problem.stop_index_to_id.size();
  problem.stop_index_to_id.push_back(stop_id);
  problem.segments.push_back({});
  return problem.stop_index_to_id.size() - 1;
}

static Problem BuildProblem(const World& world) {
  Problem problem;

  for (const auto& world_segment : world.segments) {
    size_t origin_stop_index = GetOrAddStop(world_segment.origin_stop_id, problem);
    size_t destination_stop_index = GetOrAddStop(world_segment.destination_stop_id, problem);

    GroupedSegments* segments = nullptr;
    for (GroupedSegments& candidate : problem.segments[origin_stop_index]) {
      if (candidate.destination_stop_index == destination_stop_index) {
        segments = &candidate;
        break;
      }
    }
    if (segments == nullptr) {
      problem.segments[origin_stop_index].push_back({destination_stop_index, {}});
      segments = &problem.segments[origin_stop_index].back();
    }

    segments->segments.push_back({
      .departure_time = world_segment.departure_time,
      .arrival_time = WorldTime(world_segment.departure_time.seconds + world_segment.duration.seconds),
    });
  }

  return problem;
}

constexpr int kMaxStops = 16;

struct RouteSearchState {
  // visited_at_stop[i] is the set of all visited stations the last time the search visited stop i.
  std::vector<std::bitset<kMaxStops>> visited_at_stop;

  // TODO: Explain what this is all about and also think about whether it is correct.
  std::vector<std::bitset<kMaxStops>> loop_aborts;
};

struct CollectorRouteVisitor {
  std::vector<std::vector<size_t>> routes;
  std::vector<size_t> current;

  void PushStop(size_t index) {
    current.push_back(index);
  }

  void PopStop() {
    current.pop_back();
  }

  void RouteEnd() {
    routes.push_back(current);
  }
};

template<typename RouteVisitor>
static void FindAllRoutesRec(
  RouteVisitor& visitor,
  const Problem& problem,
  const std::bitset<kMaxStops> target_visited,
  RouteSearchState& state,
  size_t current_stop,
  const std::bitset<kMaxStops> current_visited
) {
  visitor.PushStop(current_stop);

  if (current_visited == target_visited) {
    visitor.RouteEnd();
    visitor.PopStop();
    return;
  }

  std::vector<std::bitset<kMaxStops>> old_loop_aborts = state.loop_aborts;  // TODO: could probably avoid copy
  const std::bitset<kMaxStops> old_visited_at_current_stop = state.visited_at_stop[current_stop];
  state.visited_at_stop[current_stop] = current_visited;
  if (old_visited_at_current_stop.any()) {
    state.loop_aborts.push_back(current_visited & (~old_visited_at_current_stop));
  }

  for (std::bitset<kMaxStops>& loop_abort : state.loop_aborts) {
    loop_abort[current_stop] = false;
    if (loop_abort.none()) {
        state.visited_at_stop[current_stop] = old_visited_at_current_stop;
        state.loop_aborts = old_loop_aborts;
        visitor.PopStop();
        return;
    }
  }

  for (const GroupedSegments& edge : problem.segments[current_stop]) {
    size_t next_stop = edge.destination_stop_index;
    std::bitset<kMaxStops> next_visited = current_visited;
    next_visited[next_stop] = true;
    FindAllRoutesRec(visitor, problem, target_visited, state, next_stop, next_visited);
  }

  state.visited_at_stop[current_stop] = old_visited_at_current_stop;
  state.loop_aborts = old_loop_aborts;
  visitor.PopStop();
}

template<typename RouteVisitor>
void FindAllRoutes(RouteVisitor &visitor, const Problem& problem, size_t start) {
  RouteSearchState state;
  state.visited_at_stop.resize(problem.stop_index_to_id.size());

  std::bitset<kMaxStops> target_visited;
  for (size_t i = 0; i < problem.stop_index_to_id.size(); i++) {
    target_visited[i] = true;
  }

  std::bitset<kMaxStops> start_visited;
  start_visited[start] = true;

  FindAllRoutesRec(visitor, problem, target_visited, state, start, start_visited);
}

void Solve(const World& world, const std::string& start_stop_id) {
  Problem problem = BuildProblem(world);
  std::cout << "Built problem with " << problem.stop_index_to_id.size() << " stops.\n";

  CollectorRouteVisitor visitor;
  FindAllRoutes(visitor, problem, problem.stop_id_to_index.at(start_stop_id));
  std::cout << "Found " << visitor.routes.size() << " routes.\n";

  // ALMOST THERE!
  // The next thing to do is for each route step through and match up all departure/arrivals to find
  // all the minimal trips along the route.
  }
