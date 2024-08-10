#include "Solver.h"

#include <iostream>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_join.h"

#include "Problem.h"
#include "WalkFinder.h"

static std::vector<Segment> GetMinimalConnectingSegments(
  const std::vector<Segment>& a,
  const std::vector<Segment>& b,
  const unsigned int min_transfer_seconds
) {
  std::vector<Segment> result;

  // TODO: Think harder about whether this is really correct.
  // Like you might have an earlier segment that is worse than a later one because it goes faster or
  // because it's on a different/same trip_id.
  
  auto get_min_transfer_seconds = [&a, &b, min_transfer_seconds](size_t ai, size_t bi) -> unsigned int {
    if (a[ai].arrival_trip_index == b[bi].departure_trip_index) {
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
      result.push_back({
        a[a_index].departure_time,
        b[b_index].arrival_time,
        {},
        a[a_index].departure_trip_index,
        b[b_index].arrival_trip_index
      });
    }
    ++a_index;
  }

  return result;
}

static Schedule GetMinimalConnectingSchedule(
  const Schedule& a,
  const Schedule& b,
  const unsigned int min_transfer_seconds
) {
  if ((!a.segments.empty() && a.anytime_duration.has_value()) || (!b.segments.empty() && b.anytime_duration.has_value())) {
    throw std::runtime_error("Cannot deal with schedules that have segments and anytime connections.");
  }
  if (a.anytime_duration.has_value() && b.anytime_duration.has_value()) {
    return {
      .segments = {},
      .anytime_duration = WorldDuration(a.anytime_duration->seconds + b.anytime_duration->seconds),
    };
  }
  if (a.anytime_duration.has_value()) {
    Schedule result;
    result.segments = b.segments;
    for (Segment& segment : result.segments) {
      segment.departure_time = WorldTime(segment.departure_time.seconds - a.anytime_duration->seconds);
      segment.departure_trip_index = 0;
    }
    return result;
  }
  if (b.anytime_duration.has_value()) {
    Schedule result;
    result.segments = a.segments;
    for (Segment& segment : result.segments) {
      segment.arrival_time = WorldTime(segment.arrival_time.seconds + b.anytime_duration->seconds);
      segment.arrival_trip_index = 0;
    }
    return result;
  }
  Schedule result;
  result.segments = GetMinimalConnectingSegments(a.segments, b.segments, min_transfer_seconds);
  return result;
}

struct BestWalk {
  std::vector<size_t> walk;
  std::vector<WorldTime> start_times;
};

struct SolverWalkVisitorState {
  // The stop we are currently at.
  size_t stop_index;

  // The "schedule" from the starting stop to the current stop.
  Schedule schedule;
};

static const Schedule* GetSchedule(
  const Problem& problem,
  size_t origin_stop_index,
  size_t destination_stop_index
) {
  for (const Edge& edge : problem.edges[origin_stop_index]) {
    if (edge.destination_stop_index == destination_stop_index) {
      return &edge.schedule;
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

    // Push a state with the curent stop, and a "dummy" schedule.
    // We will fill in this schedule appropriately and return.
    stack.push_back({stop_index, {}});
    SolverWalkVisitorState& state = stack.back();

    if (stack.size() == 1) {
      // This is the first stop, so we can get to it "anytime", and it takes 0 minutes.
      state.schedule.anytime_duration = WorldDuration(0);
      return true;
    }

    const SolverWalkVisitorState& prev_state = stack[stack.size() - 2];
    const Schedule* schedule = GetSchedule(problem, prev_state.stop_index, stop_index);
    if (schedule == nullptr) {
      // There is no schedule between these stops, so prune.
      return false;
    }

    // const unsigned int min_transfer_seconds = (
    //   (problem.stop_id_to_index.at("bart-place_COLS") == prev_state.stop_index ||
    //    problem.stop_id_to_index.at("bart-place_RICH") == prev_state.stop_index) ? 1 * 60 : 0 * 60
    // );
    const unsigned int min_transfer_seconds = 0;
    state.schedule = GetMinimalConnectingSchedule(prev_state.schedule, *schedule, min_transfer_seconds);

    const unsigned int captured_best_duration = best_duration;
    std::erase_if(state.schedule.segments, [captured_best_duration](const Segment& segment) {
      return segment.arrival_time.seconds - segment.departure_time.seconds > captured_best_duration;
    });
    if (state.schedule.anytime_duration.has_value() && state.schedule.anytime_duration->seconds > best_duration) {
      state.schedule.anytime_duration = std::nullopt;
    }

    // Prune iff the schedule has become empty.
    return !(state.schedule.segments.empty() && !state.schedule.anytime_duration.has_value());
  }

  void PopStop() {
    stack.pop_back();
  }

  void WalkDone() {
    if (stack.size() == 0) {
      // TODO: Actually in this case, the empty walk or the anytime-only walk is probably actually a
      // solution and we should account for it.
      return;
    }

    bool pushed_this_walk = false;
    for (const Segment& segment : stack.back().schedule.segments) {
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

static void AssertSymmetric(const AdjacencyList& adjacency_list) {
  AdjacencyList transposed;
  transposed.edges.resize(adjacency_list.edges.size());
  for (size_t i = 0; i < adjacency_list.edges.size(); ++i) {
    for (size_t j : adjacency_list.edges[i]) {
      transposed.edges[j].push_back(i);
    }
  }
  for (size_t i = 0; i < adjacency_list.edges.size(); ++i) {
    auto a = adjacency_list.edges[i];
    auto b = transposed.edges[i];
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    if (a != b) {
      throw std::runtime_error("Adjacency list is not symmetric.");
    }
  }
}

// Deletes all degree-2 stops and creates combined segments for them.
// This is "naive" because a solution to the pruned problem might not solve the original problem.
static Problem NaivePruneProblem(const Problem& problem) {
  AssertSymmetric(problem.adjacency_list);

  Problem pruned;

  // The trips stay the same.
  pruned.trip_id_to_index = problem.trip_id_to_index;
  pruned.trip_index_to_id = problem.trip_index_to_id;

  // Figure out which stops to keep.
  std::unordered_map<size_t, size_t> old_to_new_stop_index;
  std::unordered_map<size_t, size_t> new_to_old_stop_index;
  {
    size_t new_stop_index = 0;
    for (size_t old_stop_index = 0; old_stop_index < problem.adjacency_list.edges.size(); ++old_stop_index) {
      if (problem.adjacency_list.edges[old_stop_index].size() != 2) {
        std::cout << "Keeping stop: " << problem.stop_index_to_id[old_stop_index] << "\n";
        for (const size_t adj_i : problem.adjacency_list.edges[old_stop_index]) {
          std::cout << "  Adjacent stop: " << problem.stop_index_to_id[adj_i] << "\n";
        }

        // Keep the stop!
        old_to_new_stop_index[old_stop_index] = new_stop_index;
        new_to_old_stop_index[new_stop_index] = old_stop_index;
        ++new_stop_index;
      } else {
        std::cout << "Pruned stop: " << problem.stop_index_to_id[old_stop_index] << "\n";
      }
    }
  }

  // Now we can build a few more things.
  pruned.stop_id_to_index.reserve(new_to_old_stop_index.size());
  pruned.stop_index_to_id.resize(new_to_old_stop_index.size());
  for (size_t new_stop_index = 0; new_stop_index < pruned.stop_index_to_id.size(); ++new_stop_index) {
    const size_t old_stop_index = new_to_old_stop_index[new_stop_index];
    pruned.stop_id_to_index[problem.stop_index_to_id[old_stop_index]] = new_stop_index;
    pruned.stop_index_to_id[new_stop_index] = problem.stop_index_to_id[old_stop_index];
  }

  // Ok so now we need to build the adjacency list and the segments and the "anytime connections".
  // This is a bit tricky because we might have pruned adjacent stops, so we kinda need to traverse
  // the whole pruned sections to construct the new stuff.
  // We're gonna do this by starting at each non-pruned "new" stop and looking for all the stuff it
  // connects to, allowing multiple hops through pruned stops.
  pruned.edges.resize(new_to_old_stop_index.size());
  pruned.adjacency_list.edges.resize(new_to_old_stop_index.size());
  for (size_t new_stop_index = 0; new_stop_index < pruned.stop_index_to_id.size(); ++new_stop_index) {
    const size_t old_stop_index = new_to_old_stop_index[new_stop_index];
    for (const Edge& edge : problem.edges[old_stop_index]) {
      size_t old_origin_stop_index = old_stop_index;
      size_t old_destination_stop_index = edge.destination_stop_index;
      Schedule old_schedule = edge.schedule;
      while (!old_to_new_stop_index.contains(old_destination_stop_index)) {
        // Oh no, this edge goes into a pruned stop. EXTEND!!
        const Edge* next_edge = nullptr;
        for (const Edge& next_edge_candidate : problem.edges[old_destination_stop_index]) {
          if (next_edge_candidate.destination_stop_index != old_origin_stop_index) {
            next_edge = &next_edge_candidate;
            break;
          }
        }
        if (next_edge == nullptr) {
          std::cout << "old stop: " << problem.stop_index_to_id[old_stop_index] << "\n";
          std::cout << "old origin stop: " << problem.stop_index_to_id[old_origin_stop_index] << "\n";
          std::cout << "old destination stop: " << problem.stop_index_to_id[old_destination_stop_index] << "\n";

          throw std::runtime_error("No next edge found. This should never happen.");
        }

        // TODO: Implement min_transfer_seconds here.
        old_schedule = GetMinimalConnectingSchedule(old_schedule, next_edge->schedule, 0);
        old_origin_stop_index = old_destination_stop_index;
        old_destination_stop_index = next_edge->destination_stop_index;
      }

      // Okay we should have extended to a non-pruned stop, so we're ready to insert the edge!!
      pruned.edges[new_stop_index].push_back({
        .destination_stop_index = old_to_new_stop_index[old_destination_stop_index],
        .schedule = old_schedule,
      });
      pruned.adjacency_list.edges[new_stop_index].push_back(old_to_new_stop_index[old_destination_stop_index]);
    }
  }

  std::cout << "pruning done\n";
  return pruned;
}

struct PrettyPrintWalkState {
  WorldTime arrival_time;
  std::optional<WorldTime> departure_time;
  std::optional<size_t> departure_trip_index;
};

void Solve(
  const World& world,
  const Problem& problem,
  const std::vector<std::string>& target_stop_ids
) {
  std::bitset<64> target_stops;
  for (const std::string& stop_id : target_stop_ids) {
    if (problem.stop_id_to_index.contains(stop_id)) {
      target_stops[problem.stop_id_to_index.at(stop_id)] = true;
    }
  }
  if (target_stops.none()) {
    std::cout << "No target stops.\n";
    return;
  }

  std::cout << "Solving...\n";

  SolverWalkVisitor visitor{.problem = problem, .best_duration = 5 * 3600 + 35 * 60};
  FindAllMinimalWalksDFS<SolverWalkVisitor, 64>(
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
      const Schedule* next_schedule = GetSchedule(problem, current_stop_index, next_stop_index);
      if (next_schedule == nullptr) {
        throw std::runtime_error("No schedule found. This should never happen.");
      }
      if (!next_schedule->segments.empty()) {
        walk_states.push_back({});
        std::vector<PrettyPrintWalkState>& current_states = walk_states[walk_states.size() - 2];
        std::vector<PrettyPrintWalkState>& next_states = walk_states.back();
        for (PrettyPrintWalkState& current_state : current_states) {
          for (const Segment& segment : next_schedule->segments) {
            // TODO: Implement min_transfer_seconds here.
            if (segment.departure_time.seconds >= current_state.arrival_time.seconds) {
              current_state.departure_time = segment.departure_time;
              current_state.departure_trip_index = segment.departure_trip_index;
              next_states.push_back({segment.arrival_time, segment.departure_trip_index});
              break;
            }
          }
        }
        continue;
      }
      if (next_schedule->anytime_duration.has_value()) {
        walk_states.push_back({});
        std::vector<PrettyPrintWalkState>& current_states = walk_states[walk_states.size() - 2];
        std::vector<PrettyPrintWalkState>& next_states = walk_states.back();
        for (PrettyPrintWalkState& current_state : current_states) {
          current_state.departure_time = current_state.arrival_time;
          current_state.departure_trip_index = 0;
          next_states.push_back({
            WorldTime(current_state.departure_time->seconds + next_schedule->anytime_duration->seconds),
            std::nullopt
          });
        }
        continue;
      }
      throw std::runtime_error("No segments or anytime duration. This should never happen.");
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
