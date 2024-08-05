#include "Simplifier.h"

#include <optional>
#include <queue>

#include "absl/strings/str_cat.h"

namespace {

struct Breadcrumb {
  size_t previous_stop_index;
  WorldTime departure_time;
  size_t trip_index;
};

struct TimeLoc {
  WorldTime time;
  size_t stop_index;
  std::optional<Breadcrumb> breadcrumb;
};

struct TimeLocCompare {
 bool operator()(const TimeLoc& l, const TimeLoc& r) {
  return l.time.seconds > r.time.seconds;
 }
};

void AddSegmentsFromDeparture(
  const Problem& original,
  TimeLoc start,
  const std::unordered_set<size_t>& keep_stop_indexes,
  Problem& new_problem
) {
  // For each stop that has been visited, how we got to it.
  std::unordered_map<size_t, TimeLoc> visited;
  visited[start.stop_index] = start;

  std::unordered_set<size_t> visited_keep_stops;
  if (keep_stop_indexes.find(start.stop_index) != keep_stop_indexes.end()) {
    visited_keep_stops.insert(start.stop_index);
  }

  std::priority_queue<TimeLoc, std::vector<TimeLoc>, TimeLocCompare> q;
  q.push(start);

  while (!q.empty() && visited_keep_stops.size() < keep_stop_indexes.size()) {
    TimeLoc cur = q.top();
    q.pop();
    visited[cur.stop_index] = cur;
    if (keep_stop_indexes.find(cur.stop_index) != keep_stop_indexes.end()) {
      visited_keep_stops.insert(cur.stop_index);
    }

    const std::vector<Edge>& outgoing_edges = original.edges[cur.stop_index];
    for (const Edge& edge : outgoing_edges) {
      if (visited.find(edge.destination_stop_index) != visited.end()) {
        continue;
      }
      if (edge.schedule.anytime_duration.has_value()) {
        throw std::runtime_error("TODO: handle anytime connections");
      }
      // TODO: Could probably sort things in such a way that I can do some kind of binary search instead of looking at all of them.
      std::optional<TimeLoc> best_arrival;
      for (const Segment& segment : edge.schedule.segments) {
        // TODO: Handle min connection times.
        if (segment.departure_time.seconds < cur.time.seconds) {
          continue;
        }
        if (!best_arrival.has_value() || segment.arrival_time.seconds < best_arrival->time.seconds) {
          best_arrival = TimeLoc{
            .time = segment.arrival_time,
            .stop_index = edge.destination_stop_index,
            .breadcrumb = Breadcrumb{
              .previous_stop_index = cur.stop_index,
              .departure_time = segment.departure_time,
              .trip_index = segment.departure_trip_index,
            }
          };
        }
      }
      if (best_arrival.has_value()) {
        q.push(*best_arrival);
      }
    }
  }

  // for (const size_t final_stop_index : keep_stop_indexes)

  // for (const auto& [stop_index, time_loc] : visited) {
  //   std::cout << original.stop_index_to_id[stop_index] << " " << absl::StrCat(time_loc.time) << "\n";
  //   if (time_loc.breadcrumb.has_value()) {
  //     std::cout << "  " << original.stop_index_to_id[time_loc.breadcrumb->previous_stop_index] << " " << original.trip_index_to_id[time_loc.breadcrumb->trip_index] << "\n";
  //   }
  // }
}


void AddSegmentsFromStop(
  const Problem& original,
  size_t start_stop_index,
  const std::unordered_set<size_t>& keep_stop_indexes,
  Problem& new_problem
) {
}

};  // namespace

Problem SimplifyProblem(const Problem& problem, const std::vector<std::string>& keep_stop_ids) {
  Problem new_problem;
  // for (const std::string& stop_id : keep_stop_ids) {
    // AddSegmentsFromStop(problem, stop_id, keep_stop_ids, new_problem);
  // }

  std::unordered_set<size_t> keep_stop_indexes;
  for (const std::string& stop_id : keep_stop_ids) {
    keep_stop_indexes.insert(problem.stop_id_to_index.at(stop_id));
  }

  TimeLoc start{
    .time = WorldTime(10 * 3600),
    .stop_index = problem.stop_id_to_index.at("bart-place_BERY"),
    .breadcrumb = std::nullopt
  };

  AddSegmentsFromDeparture(problem, start, keep_stop_indexes, new_problem);

  // For each keep_stop_id.
  // For each departure time.
  // Dijkstra, terminating as soon as we have visited all the keep_stop_ids.
  // For each found route, create a segment from the start point to the first keep_stop_id that the route hits.
  // Make sure to dedupe things cuz there are going to be dups for many reasons.


  return Problem();
}
