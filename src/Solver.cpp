#include "Solver.h"

#include <iostream>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_join.h"

#include "WalkFinder.h"

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

  // This is the projection of segments to just a graph on stops.
  AdjacencyList adjacency_list;
};

static size_t GetOrAddStop(const std::string& stop_id, Problem& problem) {
  if (problem.stop_id_to_index.contains(stop_id)) {
    return problem.stop_id_to_index.at(stop_id);
  }
  problem.stop_id_to_index[stop_id] = problem.stop_index_to_id.size();
  problem.stop_index_to_id.push_back(stop_id);
  problem.segments.push_back({});
  problem.adjacency_list.edges.push_back({});
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
      problem.adjacency_list.edges[origin_stop_index].push_back(destination_stop_index);
    }

    segments->segments.push_back({
      .departure_time = world_segment.departure_time,
      .arrival_time = WorldTime(world_segment.departure_time.seconds + world_segment.duration.seconds),
    });
  }

  return problem;
}

static std::vector<Segment> GetMinimalConnections(
  const std::vector<Segment>& a,
  const std::vector<Segment>& b
) {
  std::vector<Segment> result;

  size_t a_index = 0;
  size_t b_index = 0;
  while (a_index < a.size()) {
    while (b_index < b.size() & b[b_index].departure_time.seconds < a[a_index].arrival_time.seconds) {
      ++b_index;
    }
    if (b_index == b.size()) {
      break;
    }
    if (a_index == a.size() - 1 || a[a_index + 1].arrival_time.seconds > b[b_index].departure_time.seconds) {
      result.push_back({a[a_index].departure_time, b[b_index].arrival_time});
    }
    ++a_index;
  }

  return result;
}

void Solve(
  const World& world,
  const std::vector<std::string>& target_stop_ids
) {
  std::cout << "Building problem...\n";
  Problem problem = BuildProblem(world);
  std::cout << "Built problem with " << problem.stop_index_to_id.size() << " stops.\n";

  std::bitset<32> target_stops;
  for (const std::string& stop_id : target_stop_ids) {
    target_stops[problem.stop_id_to_index.at(stop_id)] = true;
  }
  if (target_stops.none()) {
    std::cout << "No target stops.\n";
    return;
  }

  CollectorWalkVisitor visitor;
  FindAllMinimalWalksDFS<CollectorWalkVisitor, 32>(
    visitor,
    problem.adjacency_list,
    target_stops
  );
  std::cout << "Found " << visitor.walks.size() << " walks.\n";

  unsigned int best_duration = std::numeric_limits<unsigned int>::max();
  std::optional<Segment> best_segment;
  std::optional<std::vector<size_t>> best_route;

  for (const std::vector<size_t>& route : visitor.walks) {
    std::optional<std::vector<Segment>> current_segments;
    for (size_t i = 1; i < route.size(); ++i) {
      const size_t origin = route[i - 1];
      const size_t destination = route[i];

      const std::vector<Segment>* next_segments = nullptr;
      for (const GroupedSegments& edge : problem.segments[origin]) {
        if (edge.destination_stop_index == destination) {
          next_segments = &edge.segments;
          break;
        }
      }

      if (next_segments == nullptr) {
        current_segments = {};
        break;
      }

      if (current_segments.has_value()) {
        current_segments = GetMinimalConnections(*current_segments, *next_segments);
      } else {
        current_segments = *next_segments;
      }
    }

    if (current_segments.has_value()) {
      for (const Segment& segment : *current_segments) {
        const unsigned int duration = segment.arrival_time.seconds - segment.departure_time.seconds;
        if (duration < best_duration) {
          best_duration = duration;
          best_segment = segment;
          best_route = route;
        }
      }
    }
  }

  std::cout << "Best duration: " << absl::StrCat(WorldDuration(best_duration), "\n");

  std::vector<std::string> route_stops;
  for (size_t stop_index : *best_route) {
    route_stops.push_back(problem.stop_index_to_id[stop_index]);
  }
  std::cout << absl::StrJoin(route_stops, " -> ") << "\n";
  std::cout << "Departure: " << absl::StrCat(best_segment->departure_time, "") << "\n";
}
