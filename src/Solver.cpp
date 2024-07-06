#include "Solver.h"

#include <iostream>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_join.h"

#include "WalkFinder.h"

struct Segment {
  WorldTime departure_time;
  WorldTime arrival_time;

  size_t trip_index;
};

struct GroupedSegments {
  size_t destination_stop_index;
  std::vector<Segment> segments;
};

struct AnytimeConnection {
  size_t destination_stop_index;
  WorldDuration duration;
};

struct Problem {
  // Mappings between World and Problem ids/indices.
  absl::flat_hash_map<std::string, size_t> stop_id_to_index;
  std::vector<std::string> stop_index_to_id;
  absl::flat_hash_map<std::string, size_t> trip_id_to_index;
  std::vector<std::string> trip_index_to_id;

  // segments[i] are all the segments originating from stop (index) i.
  std::vector<std::vector<GroupedSegments>> segments;

  // anytime_connections[i] are all the anytime connections originating from stop (index) i.
  std::vector<std::vector<AnytimeConnection>> anytime_connections;

  // This is the projection of segments + anytime connections to just a graph on stops.
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
  problem.anytime_connections.push_back({});
  return problem.stop_index_to_id.size() - 1;
}

static size_t GetOrAddTrip(const std::string& trip_id, Problem& problem) {
  if (problem.trip_id_to_index.contains(trip_id)) {
    return problem.trip_id_to_index.at(trip_id);
  }
  problem.trip_id_to_index[trip_id] = problem.trip_index_to_id.size();
  problem.trip_index_to_id.push_back(trip_id);
  return problem.trip_index_to_id.size() - 1;
}

static Problem BuildProblem(const World& world) {
  Problem problem;

  // Reserve trip_id = 0 for anytime connections.
  GetOrAddTrip("anytime", problem);

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
      .trip_index = GetOrAddTrip(world_segment.trip_id, problem),
    });
  }

  for (const auto& anytime_connection : world.anytime_connections) {
    size_t origin_stop_index = GetOrAddStop(anytime_connection.origin_stop_id, problem);
    size_t destination_stop_index = GetOrAddStop(anytime_connection.destination_stop_id, problem);

    problem.anytime_connections[origin_stop_index].push_back({
      .destination_stop_index = destination_stop_index,
      .duration = anytime_connection.duration,
    });

    std::vector<size_t>& adj_list_edges = problem.adjacency_list.edges[origin_stop_index];
    if (std::count(adj_list_edges.begin(), adj_list_edges.end(), destination_stop_index) > 0) {
      throw std::runtime_error("Can't (yet) handle anytime connections that are also segments.");
    }
    adj_list_edges.push_back(destination_stop_index);
  }

  return problem;
}

static std::vector<Segment> GetMinimalConnections(
  const std::vector<Segment>& a,
  const std::vector<Segment>& b,
  const unsigned int min_transfer_seconds
) {
  std::vector<Segment> result;

  // TODO: Think harder about whether this is really correct.
  // Like you might have an earlier segment that is worse than a later one because it goes faster or
  // because it's on a different/same trip_id.
  
  auto get_min_transfer_seconds = [&a, &b, min_transfer_seconds](size_t ai, size_t bi) -> unsigned int {
    if (a[ai].trip_index == b[bi].trip_index) {
      return 0;
    }
    return min_transfer_seconds;
  };

  size_t a_index = 0;
  size_t b_index = 0;
  while (a_index < a.size()) {
    while (
      (b_index < b.size()) &&
      (b[b_index].departure_time.seconds < a[a_index].arrival_time.seconds + get_min_transfer_seconds(a_index, b_index)) 
     ) {
      ++b_index;
    }
    if (b_index == b.size()) {
      break;
    }
    if (
      (a_index == a.size() - 1) || 
      (a[a_index + 1].arrival_time.seconds + get_min_transfer_seconds(a_index + 1, b_index) > b[b_index].departure_time.seconds)
    ) {
      result.push_back({a[a_index].departure_time, b[b_index].arrival_time, b[b_index].trip_index});
    }
    ++a_index;
  }

  return result;
}

struct BestWalk {
  std::vector<size_t> walk;
  std::vector<WorldTime> start_times;
};

struct SolverWalkVisitorState {
  size_t stop_index;

  // When this has no value, it means that you can take any segment out of this stop. This can
  // happen on the start, and can also happen if we take anytime connection(s) from the start.
  // Though we currently don't allow taking anytime connection(s) from the start -- we'd need to
  // keep track of their duration somewhere to support that.
  std::optional<std::vector<Segment>> segments;
};

static const std::vector<Segment>* GetSegments(
  const Problem& problem,
  size_t origin_stop_index,
  size_t destination_stop_index
) {
  for (const GroupedSegments& edge : problem.segments[origin_stop_index]) {
    if (edge.destination_stop_index == destination_stop_index) {
      return &edge.segments;
    }
  }
  return nullptr;
}

static const AnytimeConnection* GetAnytimeConnection(
  const Problem& problem,
  size_t origin_stop_index,
  size_t destination_stop_index
) {
  for (const AnytimeConnection& connection : problem.anytime_connections[origin_stop_index]) {
    if (connection.destination_stop_index == destination_stop_index) {
      return &connection;
    }
  }
  return nullptr;
}

struct SolverWalkVisitor {
  const Problem& problem;

  unsigned int best_duration = std::numeric_limits<unsigned int>::max();
  std::vector<BestWalk> best_walks;

  std::vector<SolverWalkVisitorState> stack;

  // If this returns false, this branch of the DFS is pruned.
  // The DFS will always pop the stop, even if this returns false.
  bool PushStop(size_t stop_index) {
    stack.push_back({stop_index, std::nullopt});
    SolverWalkVisitorState& state = stack.back();

    if (stack.size() == 1) {
      state.segments = std::nullopt;
      return true;
    }

    const SolverWalkVisitorState& prev_state = stack[stack.size() - 2];
    const std::vector<Segment>* prev_to_cur_segments = GetSegments(problem, prev_state.stop_index, stop_index);
    if (prev_to_cur_segments == nullptr) {
      const AnytimeConnection* anytime_connection = GetAnytimeConnection(problem, prev_state.stop_index, stop_index);
      if (anytime_connection == nullptr) {
        // There are no segments or anytime connections, so prune.
        state.segments = {};
        return false;
      }

      // Handle the anytime connection!
      if (prev_state.segments.has_value()) {
        state.segments = prev_state.segments;
        for (Segment& segment : *state.segments) {
          segment.arrival_time = WorldTime(segment.arrival_time.seconds + anytime_connection->duration.seconds);
          segment.trip_index = 0;
        }
      } else {
        // We don't support anytime connections from the start. Prune.
        state.segments = {};
        return false;
      }
    } else {
      // Handle the segments!
      if (prev_state.segments.has_value()) {
        // TODO: Make this configurable.
        const unsigned int min_transfer_seconds = (
          problem.stop_id_to_index.at("bart-place_COLS") == prev_state.stop_index ? 1 * 60 : 0 * 60
        );
        state.segments = GetMinimalConnections(
          prev_state.segments.value(),
          *prev_to_cur_segments,
          min_transfer_seconds
        );
      } else {
        state.segments = *prev_to_cur_segments;
      }
    }

    const unsigned int captured_best_duration = best_duration;
    std::erase_if(*state.segments, [captured_best_duration](const Segment& segment) {
      return segment.arrival_time.seconds - segment.departure_time.seconds > captured_best_duration;
    });

    // Prune iff we have no segments left.
    return !state.segments->empty();
  }

  void PopStop() {
    stack.pop_back();
  }

  void WalkDone() {
    if (stack.size() == 0 || !stack.back().segments.has_value()) {
      // TODO: Actually in this case, the empty walk or the anytime-only walk is probably actually a
      // solution and we should account for it.
      return;
    }

    bool pushed_this_walk = false;
    for (const Segment& segment : *stack.back().segments) {
      const unsigned int duration = segment.arrival_time.seconds - segment.departure_time.seconds;
      if (duration < best_duration) {
        std::cout << absl::StrCat("improved duration: ", WorldDuration(duration), "\n");
        best_duration = duration;
        best_walks.clear();
        pushed_this_walk = false;
      }
      if (duration == best_duration) {
        if (!pushed_this_walk) {
          best_walks.push_back({{}, {}});
          for (const SolverWalkVisitorState& state : stack) {
            best_walks.back().walk.push_back(state.stop_index);
          }
          pushed_this_walk = true;
        }
        best_walks.back().start_times.push_back(segment.departure_time);
      }
    }
  }
};

struct PrettyPrintWalkState {
  WorldTime arrival_time;
  std::optional<WorldTime> departure_time;
  std::optional<size_t> departure_trip_index;
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

  SolverWalkVisitor visitor{.problem = problem};
  FindAllMinimalWalksDFS<SolverWalkVisitor, 32>(
    visitor,
    problem.adjacency_list,
    target_stops
  );

  std::cout << "Best duration: " << absl::StrCat(WorldDuration(visitor.best_duration), "\n");

  for (const BestWalk& best_walk : visitor.best_walks) {
    std::cout << "Start times: " << absl::StrJoin(best_walk.start_times, " ") << "\n";
    std::cout << "Route: ";
    for (size_t i = 0; i < best_walk.walk.size(); ++i) {
      std::cout << problem.stop_index_to_id[best_walk.walk[i]];
      if (i < best_walk.walk.size() - 1) {
        std::cout << " -> ";
      }
    }
    std::cout << "\n\n";

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
      if (next_segments != nullptr) {
        walk_states.push_back({});
        std::vector<PrettyPrintWalkState>& current_states = walk_states[walk_states.size() - 2];
        std::vector<PrettyPrintWalkState>& next_states = walk_states.back();
        for (PrettyPrintWalkState& current_state : current_states) {
          for (const Segment& segment : *next_segments) {
            // TODO: Implement min_transfer_seconds here.
            if (segment.departure_time.seconds >= current_state.arrival_time.seconds) {
              current_state.departure_time = segment.departure_time;
              current_state.departure_trip_index = segment.trip_index;
              next_states.push_back({segment.arrival_time, segment.trip_index});
              break;
            }
          }
        }
        continue;
      }

      const AnytimeConnection* anytime_connection = nullptr;
      for (const AnytimeConnection& connection : problem.anytime_connections[current_stop_index]) {
        if (connection.destination_stop_index == next_stop_index) {
          anytime_connection = &connection;
          break;
        }
      }
      if (anytime_connection != nullptr) {
        walk_states.push_back({});
        std::vector<PrettyPrintWalkState>& current_states = walk_states[walk_states.size() - 2];
        std::vector<PrettyPrintWalkState>& next_states = walk_states.back();
        for (PrettyPrintWalkState& current_state : current_states) {
          current_state.departure_time = current_state.arrival_time;
          current_state.departure_trip_index = 0;
          next_states.push_back({
            WorldTime(current_state.departure_time->seconds + anytime_connection->duration.seconds),
            std::nullopt
          });
        }
        continue;
      }

      throw std::runtime_error("No edge found. This should never happen.");
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
          state.departure_trip_index != walk_states[i - 1][j].departure_trip_index
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

      const bool walkBike = std::all_of(walk_states[i].begin(), walk_states[i].end(), [](const auto& state) {
        return state.departure_trip_index == 0;
      });

      int max_connection_time = std::numeric_limits<int>::min();
      int min_connection_time = std::numeric_limits<int>::max();
      for (size_t j = 0; j < arrival_times.size(); ++j) {
        if (arrival_times[j].has_value() && departure_times[j].has_value()) {
          int connection_time = ((int)departure_times[j].value().seconds) - ((int)arrival_times[j].value().seconds);
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
        if (walkBike) {
          std::cout << "   (walk/bike)\n";
        } else if (min_connection_time == max_connection_time) {
          std::cout << absl::StreamFormat("   (%dm)\n", min_connection_time / 60);
        } else {
          std::cout << absl::StreamFormat("   (%d to %dm)\n", min_connection_time / 60, max_connection_time / 60);
        }
      }
      std::cout << "\n";
    }
    std::cout << "\n";
  }
}
