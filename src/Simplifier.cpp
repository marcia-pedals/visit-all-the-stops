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
  std::unordered_set<size_t> visited_keep_stops;

  std::priority_queue<TimeLoc, std::vector<TimeLoc>, TimeLocCompare> q;
  q.push(start);

  while (!q.empty() && visited_keep_stops.size() < keep_stop_indexes.size()) {
    TimeLoc cur = q.top();
    q.pop();

    if (visited.find(cur.stop_index) != visited.end()) {
      continue;
    }

    // std::cout << absl::StrCat(visited_keep_stops.size(), " of ", keep_stop_indexes.size(), " at ", cur.time, "(start ", start.time, ")\n");
    visited[cur.stop_index] = cur;
    if (keep_stop_indexes.find(cur.stop_index) != keep_stop_indexes.end()) {
      visited_keep_stops.insert(cur.stop_index);
    }

    const std::vector<Edge>& outgoing_edges = original.edges[cur.stop_index];
    for (const Edge& edge : outgoing_edges) {
      if (visited.find(edge.destination_stop_index) != visited.end()) {
        continue;
      }
      std::optional<TimeLoc> best_arrival;

      if (edge.schedule.anytime_duration.has_value()) {
        best_arrival = TimeLoc{
          .time = WorldTime(cur.time.seconds + edge.schedule.anytime_duration->seconds),
          .stop_index = edge.destination_stop_index,
          .breadcrumb = Breadcrumb{
            .previous_stop_index = cur.stop_index,
            .departure_time = cur.time,
            .trip_index = 0
          }
        };
      }

      for (const Segment& segment : edge.schedule.segments) {
        // TODO: Handle min connection times.
        if (segment.departure_time.seconds < cur.time.seconds) {
          continue;
        }
        if (!best_arrival.has_value() || segment.arrival_time.seconds < best_arrival->time.seconds) {
          if (segment.departure_trip_index != segment.arrival_trip_index) {
            throw std::runtime_error("TODO: handle multi-trip segments");
          }
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

  // std::cout << "  Dijkstra done.\n";

  const size_t new_problem_start_stop_index = GetOrAddStop(
    original.stop_index_to_id[start.stop_index], new_problem
  );

  for (const size_t final_stop_index : keep_stop_indexes) {
    if (final_stop_index == start.stop_index || visited.find(final_stop_index) == visited.end()) {
      continue;
    }

    TimeLoc cur = visited[final_stop_index];
    TimeLoc latest_at_keep = cur;
    std::vector<std::string> trips_from_latest_at_keep;
    // std::cout << "  Backtracking from " << original.stop_index_to_id[final_stop_index] << " to " << original.stop_index_to_id[start.stop_index] << "\n";
    while (cur.breadcrumb->previous_stop_index != start.stop_index) {
      // if (cur.stop_index != 0) {
      //   std::cout << "  " << original.stop_index_to_id[cur.stop_index] << "\n";
      // }
      trips_from_latest_at_keep.push_back(original.trip_index_to_id[cur.breadcrumb->trip_index]);
      cur = visited[cur.breadcrumb->previous_stop_index];
      if (keep_stop_indexes.find(cur.stop_index) != keep_stop_indexes.end()) {
        latest_at_keep = cur;
        trips_from_latest_at_keep.clear();
      }
    }
    trips_from_latest_at_keep.push_back(original.trip_index_to_id[cur.breadcrumb->trip_index]);
    // std::cout << "  Backtracking done.\n";

    Segment new_segment{
      .departure_time = cur.breadcrumb->departure_time,
      .arrival_time = latest_at_keep.time
    };
    for (auto it = trips_from_latest_at_keep.rbegin(); it != trips_from_latest_at_keep.rend(); ++it) {
      new_segment.trip_indices.push_back(GetOrAddTrip(*it, new_problem));
    }
    new_segment.departure_trip_index = new_segment.trip_indices.front();
    new_segment.arrival_trip_index = new_segment.trip_indices.back();

    const size_t new_problem_dest_stop_index = GetOrAddStop(
      original.stop_index_to_id[latest_at_keep.stop_index], new_problem
    );
    Edge* new_problem_edge = GetOrAddEdge(
      new_problem_start_stop_index,
      new_problem_dest_stop_index,
      new_problem
    );
    auto& segments = new_problem_edge->schedule.segments;
    if (std::find(segments.begin(), segments.end(), new_segment) == segments.end()) {
      segments.push_back(new_segment);
    }
  }

  // for (const auto& [stop_index, time_loc] : visited) {
  //   std::cout << original.stop_index_to_id[stop_index] << " " << absl::StrCat(time_loc.time) << "\n";
  //   if (time_loc.breadcrumb.has_value()) {
  //     std::cout << "  " << original.stop_index_to_id[time_loc.breadcrumb->previous_stop_index] << " " << original.trip_index_to_id[time_loc.breadcrumb->trip_index] << "\n";
  //   }
  // }
}

};  // namespace

Problem SimplifyProblem(const Problem& problem, const std::vector<std::string>& keep_stop_ids) {
  // For each keep_stop_id.
  // For each departure time.
  // Dijkstra, terminating as soon as we have visited all the keep_stop_ids.
  // For each found route, create a segment from the start point to the first keep_stop_id that the route hits.
  // Make sure to dedupe things cuz there are going to be dups for many reasons.

  Problem new_problem;

  std::unordered_set<size_t> keep_stop_indexes;
  for (const std::string& stop_id : keep_stop_ids) {
    // std::cout << stop_id << "\n";
    keep_stop_indexes.insert(problem.stop_id_to_index.at(stop_id));
  }


  size_t num_done = 0;
  std::cout << std::unitbuf;
  for (const size_t keep_stop_index : keep_stop_indexes) {
    std::cout << "Doing starting from " << problem.stop_index_to_id[keep_stop_index] << "(" << num_done << ")\n";
    for (const Edge& edge : problem.edges[keep_stop_index]) {
      // std::cout << "Doing to " << problem.stop_index_to_id[edge.destination_stop_index] << "\n";
      // TODO: Consider whether I need to handle anytime connections.
      int num_times = 0;
      for (const Segment& seg : edge.schedule.segments) {
        num_times += 1;
        if (num_times % 50 != 1) { continue; }

        // std::cout << "  Doing time " << absl::StrCat(seg.departure_time, "\n");
        AddSegmentsFromDeparture(
          problem,
          TimeLoc{.time = seg.departure_time, .stop_index = keep_stop_index},
          keep_stop_indexes,
          new_problem
        );
        // std::cout << "  Done\n";
      }
    }
    num_done += 1;
  }

  return new_problem;
}
