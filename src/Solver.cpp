#include "Solver.h"

#include <iostream>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_join.h"

#include "WalkFinder.h"

struct Segment {
  WorldTime departure_time;
  WorldTime arrival_time;

  size_t route_index;
};

struct GroupedSegments {
  size_t destination_stop_index;
  std::vector<Segment> segments;
};

struct Problem {
  // Mappings between World and Problem ids/indices.
  absl::flat_hash_map<std::string, size_t> stop_id_to_index;
  std::vector<std::string> stop_index_to_id;
  absl::flat_hash_map<std::string, size_t> route_id_to_index;
  std::vector<std::string> route_index_to_id;

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

static size_t GetOrAddRoute(const std::string& route_id, Problem& problem) {
  if (problem.route_id_to_index.contains(route_id)) {
    return problem.route_id_to_index.at(route_id);
  }
  problem.route_id_to_index[route_id] = problem.route_index_to_id.size();
  problem.route_index_to_id.push_back(route_id);
  return problem.route_index_to_id.size() - 1;
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
      .route_index = GetOrAddRoute(world_segment.route_id, problem),
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

struct BestWalk {
  std::vector<size_t> walk;
  std::vector<WorldTime> start_times;
};

struct PrettyPrintWalkState {
  WorldTime arrival_time;
  std::optional<WorldTime> departure_time;
  std::optional<size_t> departure_route_index;
};

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
  std::vector<BestWalk> best_walks;

  for (const std::vector<size_t>& walk : visitor.walks) {
    std::optional<std::vector<Segment>> current_segments;
    for (size_t i = 1; i < walk.size(); ++i) {
      const size_t origin = walk[i - 1];
      const size_t destination = walk[i];

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

    bool pushed_this_walk = false;
    if (current_segments.has_value()) {
      for (const Segment& segment : *current_segments) {
        const unsigned int duration = segment.arrival_time.seconds - segment.departure_time.seconds;
        if (duration < best_duration) {
          best_duration = duration;
          best_walks.clear();
          pushed_this_walk = false;
        }
        if (duration == best_duration) {
          if (!pushed_this_walk) {
            best_walks.push_back({walk, {}});
            pushed_this_walk = true;
          }
          best_walks.back().start_times.push_back(segment.departure_time);
        }
      }
    }
  }

  std::cout << "Best duration: " << absl::StrCat(WorldDuration(best_duration), "\n");

  for (const BestWalk& best_walk : best_walks) {
    std::vector<std::vector<PrettyPrintWalkState>> walk_states;
    walk_states.push_back({});
    for (const WorldTime& start_time : best_walk.start_times) {
      walk_states.back().push_back({start_time, std::nullopt});
    }
    for (size_t i = 1; i < best_walk.walk.size(); ++i) {
      const size_t current_stop_index = best_walk.walk[i - 1];
      const size_t next_stop_index = best_walk.walk[i];
      const std::vector<Segment>* next_segments = nullptr;
      for (const GroupedSegments& edge : problem.segments[current_stop_index]) {
        if (edge.destination_stop_index == next_stop_index) {
          next_segments = &edge.segments;
          break;
        }
      }
      if (next_segments == nullptr) {
        std::cout << "Error: no segments. This should never happen.\n";
        return;
      }

      walk_states.push_back({});
      std::vector<PrettyPrintWalkState>& current_states = walk_states[walk_states.size() - 2];
      std::vector<PrettyPrintWalkState>& next_states = walk_states.back();
      for (PrettyPrintWalkState& current_state : current_states) {
        for (const Segment& segment : *next_segments) {
          if (segment.departure_time.seconds >= current_state.arrival_time.seconds) {
            current_state.departure_time = segment.departure_time;
            current_state.departure_route_index = segment.route_index;
            next_states.push_back({segment.arrival_time, segment.route_index});
            break;
          }
        }
      }
    }

    auto optTimeFormatter = [](std::string *out, const std::optional<WorldTime>& time) {
      if (time.has_value()) {
        absl::StrAppend(out, time.value());
      } else {
        absl::StrAppend(out, "  |  ");
      }
    };

    for (size_t i = 0; i < best_walk.walk.size(); ++i) {
      std::vector<std::optional<WorldTime>> arrival_times, departure_times;
      for (size_t j = 0; j < walk_states[i].size(); ++j) {
        const PrettyPrintWalkState& state = walk_states[i][j];
        if (
          i == 0 ||
          i == best_walk.walk.size() - 1 ||
          state.departure_route_index != walk_states[i - 1][j].departure_route_index
        ) {
          arrival_times.push_back(state.arrival_time);
          departure_times.push_back(state.departure_time);
        } else {
          arrival_times.push_back(std::nullopt);
          departure_times.push_back(std::nullopt);
        }
      }

      if (
        std::count(arrival_times.begin(), arrival_times.end(), std::nullopt) == arrival_times.size() &&
        std::count(departure_times.begin(), departure_times.end(), std::nullopt) == departure_times.size()
      ) {
        continue;
      }

      unsigned int max_connection_time = 0;
      unsigned int min_connection_time = std::numeric_limits<unsigned int>::max();
      for (size_t j = 0; j < arrival_times.size(); ++j) {
        if (arrival_times[j].has_value() && departure_times[j].has_value()) {
          unsigned int connection_time = departure_times[j].value().seconds - arrival_times[j].value().seconds;
          max_connection_time = std::max(max_connection_time, connection_time);
          min_connection_time = std::min(min_connection_time, connection_time);
        }
      }

      std::cout << absl::StreamFormat(
        "%-40s %s\n",
        world.stops.at(problem.stop_index_to_id[best_walk.walk[i]]).name,
        absl::StrJoin(arrival_times, " ", optTimeFormatter)
      );
      if (i > 0 && i < best_walk.walk.size() - 1) {
        std::cout << absl::StreamFormat(
          "%-40s %s",
          "",
          absl::StrJoin(departure_times, " ", optTimeFormatter)
        );
        if (min_connection_time == max_connection_time) {
          std::cout << absl::StreamFormat("   (%um)\n", min_connection_time / 60);
        } else {
          std::cout << absl::StreamFormat("   (%u-%um)\n", min_connection_time / 60, max_connection_time / 60);
        }
      }
      std::cout << "\n";
    }
    std::cout << "\n";
  }
}
