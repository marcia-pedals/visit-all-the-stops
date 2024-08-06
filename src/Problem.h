#pragma once

#include <optional>
#include <vector>

#include "absl/container/flat_hash_map.h"

#include "WalkFinder.h"
#include "World.h"

struct Segment {
  WorldTime departure_time;
  WorldTime arrival_time;

  size_t departure_trip_index;
  size_t arrival_trip_index;

  bool operator==(const Segment& other) {
    return (
      departure_time == other.departure_time &&
      arrival_time == other.arrival_time &&
      departure_trip_index == other.departure_trip_index &&
      arrival_trip_index == other.arrival_trip_index
    );
  }
};

struct Schedule {
  std::vector<Segment> segments;
  std::optional<WorldDuration> anytime_duration;
};

struct Edge {
  size_t destination_stop_index;
  Schedule schedule;
};

struct Problem {
  // Mappings between World and Problem ids/indices.
  absl::flat_hash_map<std::string, size_t> stop_id_to_index;
  std::vector<std::string> stop_index_to_id;
  absl::flat_hash_map<std::string, size_t> trip_id_to_index;
  std::vector<std::string> trip_index_to_id;

  // edges[i] are all the edges originating from stop (index) i.
  std::vector<std::vector<Edge>> edges;

  // This is the projection of `edges` to just a graph on stops.
  AdjacencyList adjacency_list;
};

size_t GetOrAddStop(const std::string& stop_id, Problem& problem);
size_t GetOrAddTrip(const std::string& trip_id, Problem& problem);
Edge* GetOrAddEdge(size_t origin_stop_index, size_t destination_stop_index, Problem& problem);

Problem BuildProblem(const World& world);
