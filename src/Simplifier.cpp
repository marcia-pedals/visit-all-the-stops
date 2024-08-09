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
  WorldTime time = WorldTime(10 * 24 * 3600);
  size_t stop_index = 0;
  std::optional<Breadcrumb> breadcrumb = std::nullopt;
  bool visited = false;
};

struct HeapEntry {
  unsigned int priority;
  size_t stop_index;
};

struct HeapEntryCompare {
 bool operator()(const HeapEntry& l, const HeapEntry& r) {
  return l.priority > r.priority;
 }
};

void AddSegmentsFromDeparture(
  const Problem& original,
  TimeLoc start,
  // const std::unordered_set<size_t>& keep_stop_indexes,
  const std::vector<size_t>& keep_stop_indexes,
  const std::vector<bool>& is_keep_stop,
  Problem& new_problem
) {
  std::vector<TimeLoc> stop_state(original.edges.size());

  int num_visited_keep_stops = 0;

  std::priority_queue<HeapEntry, std::vector<HeapEntry>, HeapEntryCompare> q;
  stop_state[start.stop_index] = start;
  q.push(HeapEntry{
    .priority = start.time.seconds,
    .stop_index = start.stop_index
  });

  while (!q.empty() && num_visited_keep_stops < keep_stop_indexes.size()) {
    HeapEntry top = q.top();
    q.pop();

    TimeLoc cur = stop_state[top.stop_index];
    if (cur.visited) {
      continue;
    }

    // std::cout << absl::StrCat(visited_keep_stops.size(), " of ", keep_stop_indexes.size(), " at ", cur.time, "(start ", start.time, ")\n");
    stop_state[cur.stop_index].visited = true;
    if (is_keep_stop[cur.stop_index]) {
      num_visited_keep_stops += 1;
    }

    const std::vector<Edge>& outgoing_edges = original.edges[cur.stop_index];
    for (const Edge& edge : outgoing_edges) {
      if (stop_state[edge.destination_stop_index].visited) {
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
          },
          .visited = false
        };
      }

      auto it = std::lower_bound(
        edge.schedule.segments.begin(),
        edge.schedule.segments.end(),
        cur.time,
        [](const Segment& seg, const WorldTime& cur_time) { return seg.departure_time.seconds < cur_time.seconds; }
      );
      while (
        it != edge.schedule.segments.end() &&
        (
          // We can stop checking departures if they depart after the best arrival time that we have already found.
          !best_arrival.has_value() || it->departure_time.seconds < best_arrival->time.seconds
        )
      ) {
        if (!best_arrival.has_value() || it->arrival_time.seconds < best_arrival->time.seconds) {
          if (it->departure_trip_index != it->arrival_trip_index) {
            throw std::runtime_error("TODO: handle multi-trip segments");
          }
          best_arrival = TimeLoc{
            .time = it->arrival_time,
            .stop_index = edge.destination_stop_index,
            .breadcrumb = Breadcrumb{
              .previous_stop_index = cur.stop_index,
              .departure_time = it->departure_time,
              .trip_index = it->departure_trip_index,
            },
            .visited = false
          };
        }
        ++it;
      }

      if (best_arrival.has_value() && best_arrival->time.seconds < stop_state[best_arrival->stop_index].time.seconds) {
        stop_state[best_arrival->stop_index] = *best_arrival;
        q.push(HeapEntry{
          .priority = best_arrival->time.seconds,
          .stop_index = best_arrival->stop_index
        });
      }
    }
  }

  // std::cout << "  Dijkstra done.\n";

  const size_t new_problem_start_stop_index = GetOrAddStop(
    original.stop_index_to_id[start.stop_index], new_problem
  );

  // Trip index in original problem.
  std::vector<size_t> trips_from_latest_at_keep;
  trips_from_latest_at_keep.reserve(50);

  for (const size_t final_stop_index : keep_stop_indexes) {
    if (final_stop_index == start.stop_index || !stop_state[final_stop_index].visited) {
      continue;
    }

    TimeLoc cur = stop_state[final_stop_index];
    TimeLoc latest_at_keep = cur;
    trips_from_latest_at_keep.clear();

    // std::cout << "  Backtracking from " << original.stop_index_to_id[final_stop_index] << " to " << original.stop_index_to_id[start.stop_index] << "\n";
    while (cur.breadcrumb->previous_stop_index != start.stop_index) {
      // if (cur.stop_index != 0) {
      //   std::cout << "  " << original.stop_index_to_id[cur.stop_index] << "\n";
      // }
      if (trips_from_latest_at_keep.size() == 0 || trips_from_latest_at_keep.back() != cur.breadcrumb->trip_index) {
        trips_from_latest_at_keep.push_back(cur.breadcrumb->trip_index);
      }
      cur = stop_state[cur.breadcrumb->previous_stop_index];
      if (is_keep_stop[cur.stop_index]) {
        latest_at_keep = cur;
        trips_from_latest_at_keep.clear();
      }
    }
    if (trips_from_latest_at_keep.size() == 0 || trips_from_latest_at_keep.back() != cur.breadcrumb->trip_index) {
      trips_from_latest_at_keep.push_back(cur.breadcrumb->trip_index);
    }
    // std::cout << "  Backtracking done.\n";

    Segment new_segment{
      .departure_time = cur.breadcrumb->departure_time,
      .arrival_time = latest_at_keep.time
    };
    for (auto it = trips_from_latest_at_keep.rbegin(); it != trips_from_latest_at_keep.rend(); ++it) {
      new_segment.trip_indices.push_back(GetOrAddTrip(original.trip_index_to_id[*it], new_problem));
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

  std::vector<size_t> keep_stop_indexes;
  std::vector<bool> is_keep_stop(problem.edges.size());
  for (const std::string& stop_id : keep_stop_ids) {
    // std::cout << stop_id << "\n";
    const size_t stop_index = problem.stop_id_to_index.at(stop_id);
    keep_stop_indexes.push_back(stop_index);
    is_keep_stop[stop_index] = true;
  }


  size_t num_done = 0;
  std::cout << std::unitbuf;
  for (const size_t keep_stop_index : keep_stop_indexes) {
    std::cout << "Doing starting from " << problem.stop_index_to_id[keep_stop_index] << "(" << num_done << ")\n";
    for (const Edge& edge : problem.edges[keep_stop_index]) {
      // std::cout << "Doing to " << problem.stop_index_to_id[edge.destination_stop_index] << "\n";
      // TODO: Consider whether I need to handle anytime connections.
      // int num_times = 0;
      for (const Segment& seg : edge.schedule.segments) {
        // num_times += 1;
        // if (num_times % 10 != 1) { continue; }

        // std::cout << "  Doing time " << absl::StrCat(seg.departure_time, "\n");
        AddSegmentsFromDeparture(
          problem,
          TimeLoc{.time = seg.departure_time, .stop_index = keep_stop_index, .breadcrumb = std::nullopt, .visited = false},
          keep_stop_indexes,
          is_keep_stop,
          new_problem
        );
        // std::cout << "  Done\n";
      }
    }
    num_done += 1;
  }

  return new_problem;
}
