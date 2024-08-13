#include "Problem.h"

size_t GetOrAddStop(const std::string& stop_id, Problem& problem) {
  if (problem.stop_id_to_index.contains(stop_id)) {
    return problem.stop_id_to_index.at(stop_id);
  }
  problem.stop_id_to_index[stop_id] = problem.stop_index_to_id.size();
  problem.stop_index_to_id.push_back(stop_id);
  problem.edges.push_back({});
  problem.adjacency_list.edges.push_back({});
  return problem.stop_index_to_id.size() - 1;
}

size_t GetOrAddTrip(const std::string& trip_id, Problem& problem) {
  if (problem.trip_id_to_index.contains(trip_id)) {
    return problem.trip_id_to_index.at(trip_id);
  }
  problem.trip_id_to_index[trip_id] = problem.trip_index_to_id.size();
  problem.trip_index_to_id.push_back(trip_id);
  return problem.trip_index_to_id.size() - 1;
}

Edge* GetOrAddEdge(
  size_t origin_stop_index,
  size_t destination_stop_index,
  Problem& problem
) {
  for (Edge& edge : problem.edges[origin_stop_index]) {
    if (edge.destination_stop_index == destination_stop_index) {
      return &edge;
    }
  }
  problem.edges[origin_stop_index].push_back({destination_stop_index, {}});
  problem.adjacency_list.edges[origin_stop_index].push_back(destination_stop_index);
  return &problem.edges[origin_stop_index].back();
}

Problem BuildProblem(const World& world) {
  Problem problem;

  // Reserve trip_id = 0 for anytime connections.
  GetOrAddTrip("anytime", problem);

  for (const auto& world_segment : world.segments) {
    size_t origin_stop_index = GetOrAddStop(world_segment.origin_stop_id, problem);
    size_t destination_stop_index = GetOrAddStop(world_segment.destination_stop_id, problem);
    Edge* edge = GetOrAddEdge(origin_stop_index, destination_stop_index, problem);
    size_t trip_index = GetOrAddTrip(world_segment.trip_id, problem);
    edge->schedule.segments.push_back({
      .departure_time = world_segment.departure_time,
      .arrival_time = WorldTime(world_segment.departure_time.seconds + world_segment.duration.seconds),
      .trip_indices = {trip_index},
      .departure_trip_index = trip_index,
      .arrival_trip_index = trip_index,
    });
  }

  for (const auto& anytime_connection : world.anytime_connections) {
    size_t origin_stop_index = GetOrAddStop(anytime_connection.origin_stop_id, problem);
    size_t destination_stop_index = GetOrAddStop(anytime_connection.destination_stop_id, problem);
    Edge* edge = GetOrAddEdge(origin_stop_index, destination_stop_index, problem);
    edge->schedule.anytime_duration = anytime_connection.duration;
  }

  for (size_t i = 0; i < problem.edges.size(); ++i) {
    for (size_t j = 0; j < problem.edges[i].size(); ++j) {
      auto& segs = problem.edges[i][j].schedule.segments;
      std::sort(segs.begin(), segs.end(), [](const Segment& a, const Segment& b) { return a.departure_time.seconds < b.departure_time.seconds; });
    }
  }

  return problem;
}

static void GetMinimalConnectingSegments(
  const std::vector<Segment>& a,
  const std::vector<Segment>& b,
  const unsigned int min_transfer_seconds,
  std::vector<Segment>& result
) {
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
}

void EraseNonMinimal(Schedule& schedule) {
  // Conjecture:
  // If I iterate through backwards, keep track of the earliest arrival time so far, and delete
  // anything later than that, then I'll have the minimal segments.
  // Ok it's clearly important that if two segments have the same departure time, then the ones with the earliest arrival are last.
  // Because e.g. (5, 10), (5, 20) we'll not eliminate the (5, 20).
  // TODO: Make sure that this ordering is an invariant (it would be trivally if we maintained the
  // invariant that we only keep minimal segments, because then there would never be more than one
  // segment at the same departure time).
  // TODO: Think whether this conjecture is actually correct.
  //
  // Another TODO about minimality: In cases with a min connection time, it may be optimal to take a
  // timewise non-minimal segment that leaves you on the same trip. So maybe I should relax the
  // minimality to be > min connection time. This seems complicated but doable.
  unsigned int best_arrival = std::numeric_limits<unsigned int>::max();
  unsigned int anytime_duration = schedule.anytime_duration_or_big().seconds;
  for (size_t i = schedule.segments.size(); i > 0; --i) {
    Segment& seg = schedule.segments[i - 1];
    if (
      seg.arrival_time.seconds >= best_arrival ||
      seg.arrival_time.seconds - seg.departure_time.seconds >= anytime_duration
    ) {
      // Mark for deletion.
      seg.arrival_time.seconds = std::numeric_limits<unsigned int>::max();
    } else {
      best_arrival = seg.arrival_time.seconds;
    }
  }
  std::erase_if(schedule.segments, [](const Segment& seg) {
    return seg.arrival_time.seconds == std::numeric_limits<unsigned int>::max();
  });
  // Intentionally not removing the anytime_duration because in practice there's probably always
  // gonna be some time outside of service hours where it is the best.
}

bool SegmentComp(const Segment& a, const Segment& b) {
  return (
    (a.departure_time.seconds < b.departure_time.seconds) ||
    (a.departure_time.seconds == b.departure_time.seconds && a.arrival_time.seconds > b.arrival_time.seconds)
  );
}

Schedule GetMinimalConnectingSchedule(
  const Schedule& a,
  const Schedule& b,
  const unsigned int min_transfer_seconds
) {
  Schedule result;
  if (a.anytime_duration.has_value() && b.anytime_duration.has_value()) {
    result.anytime_duration = WorldDuration(a.anytime_duration->seconds + b.anytime_duration->seconds);
  }

  // TODO: Think harder about the reservation size, maybe I can remove the 5 fudge factor.
  result.segments.reserve(
    (a.anytime_duration.has_value() ? b.segments.size() : 0) +
    (b.anytime_duration.has_value() ? a.segments.size() : 0) +
    std::min(a.segments.size(), b.segments.size()) +
    5
  );

  if (a.anytime_duration.has_value()) {
    for (const Segment& segment : b.segments) {
      result.segments.push_back(Segment{
        .departure_time = WorldTime(segment.departure_time.seconds - a.anytime_duration->seconds),
        .departure_trip_index = 0
      });
    }
  }
  const size_t merge_part_1 = result.segments.size();

  if (b.anytime_duration.has_value()) {
    for (const Segment& segment : a.segments) {
      result.segments.push_back(Segment{
        .departure_time = WorldTime(segment.departure_time.seconds - b.anytime_duration->seconds),
        .departure_trip_index = 0
      });
    }
  }
  const size_t merge_part_2 = result.segments.size();

  GetMinimalConnectingSegments(a.segments, b.segments, min_transfer_seconds, result.segments);

  std::inplace_merge(
    result.segments.begin(),
    result.segments.begin() + merge_part_1,
    result.segments.begin() + merge_part_2,
    SegmentComp
  );
  std::inplace_merge(
    result.segments.begin(),
    result.segments.begin() + merge_part_2,
    result.segments.end(),
    SegmentComp
  );

  EraseNonMinimal(result);

  return result;
}

void MergeIntoSchedule(const Schedule& src, Schedule &dest) {
  unsigned int anytime_duration = std::min(src.anytime_duration_or_big().seconds, dest.anytime_duration_or_big().seconds);
  if (anytime_duration < std::numeric_limits<unsigned int>::max()) {
    dest.anytime_duration = WorldDuration(anytime_duration);
  }
  const size_t merge_part = dest.segments.size();
  dest.segments.reserve(src.segments.size() + dest.segments.size());
  for (const Segment& seg : src.segments) {
    dest.segments.push_back(seg);
  }
  std::inplace_merge(
    dest.segments.begin(),
    dest.segments.begin() + merge_part,
    dest.segments.end(),
    SegmentComp
  );
  EraseNonMinimal(dest);
}
