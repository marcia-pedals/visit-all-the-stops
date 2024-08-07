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

  // for (size_t i = 0; i < problem.edges.size(); ++i) {
  //   for (size_t j = 0; j < problem.edges[i].size(); ++j) {
  //     auto& segs = problem.edges[i][j].schedule.segments;
  //     std::sort(segs.begin(), segs.end(), [](const Segment& a, const Segment& b) { return a.departure_time.seconds < b.departure_time.seconds; });
  //   }
  // }

  return problem;
}
