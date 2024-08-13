#pragma once

#include <optional>
#include <vector>

#include "absl/container/flat_hash_map.h"

#include "cereal/types/optional.hpp"
#include "cereal/types/unordered_map.hpp"
#include "cereal/types/vector.hpp"

#include "WalkFinder.h"
#include "World.h"

struct Segment {
  WorldTime departure_time;
  WorldTime arrival_time;

  // Might not be populated depending on how the segment was constructed.
  // Mostly used for display purposes, because the departure and arrival trips are all you need to
  // know for solving purposes.
  std::vector<size_t> trip_indices;

  // Always populated.
  size_t departure_trip_index;
  size_t arrival_trip_index;

  template<class Archive>
  void serialize(Archive& ar) {
    ar(
      CEREAL_NVP(departure_time),
      CEREAL_NVP(arrival_time),
      CEREAL_NVP(trip_indices),
      CEREAL_NVP(departure_trip_index),
      CEREAL_NVP(arrival_trip_index)
    );
  }

  bool operator==(const Segment& other) {
    return (
      departure_time == other.departure_time &&
      arrival_time == other.arrival_time &&
      departure_trip_index == other.departure_trip_index &&
      arrival_trip_index == other.arrival_trip_index &&
      trip_indices == other.trip_indices
    );
  }
};

struct Schedule {
  std::vector<Segment> segments;
  std::optional<WorldDuration> anytime_duration;

  WorldDuration anytime_duration_or_big() const {
    return anytime_duration.value_or(WorldDuration(std::numeric_limits<unsigned int>::max()));
  }

  template<class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(segments), CEREAL_NVP(anytime_duration));
  }
};

struct Edge {
  size_t destination_stop_index;
  Schedule schedule;

  template<class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(destination_stop_index), CEREAL_NVP(schedule));
  }
};

struct Problem {
  // Mappings between World and Problem ids/indices.
  std::unordered_map<std::string, size_t> stop_id_to_index;
  std::vector<std::string> stop_index_to_id;
  std::unordered_map<std::string, size_t> trip_id_to_index;
  std::vector<std::string> trip_index_to_id;

  // edges[i] are all the edges originating from stop (index) i.
  std::vector<std::vector<Edge>> edges;

  // This is the projection of `edges` to just a graph on stops.
  AdjacencyList adjacency_list;

  template<class Archive>
  void serialize(Archive& ar) {
    ar(
      CEREAL_NVP(stop_id_to_index),
      CEREAL_NVP(stop_index_to_id),
      CEREAL_NVP(trip_id_to_index),
      CEREAL_NVP(trip_index_to_id),
      CEREAL_NVP(edges),
      CEREAL_NVP(adjacency_list)
    );
  }
};

size_t GetOrAddStop(const std::string& stop_id, Problem& problem);
size_t GetOrAddTrip(const std::string& trip_id, Problem& problem);
Edge* GetOrAddEdge(size_t origin_stop_index, size_t destination_stop_index, Problem& problem);

Problem BuildProblem(const World& world);

// The order that segments should be ordered in a schedule.
bool SegmentComp(const Segment& a, const Segment& b);

// Erases non minimal entries from `schedule`.
//
// Precondition: `schedule.segments` is sorted by departure time ascending, with ties broken sorting
// by arrival time descending.
void EraseNonMinimal(Schedule& schedule);

Schedule GetMinimalConnectingSchedule(
  const Schedule& a,
  const Schedule& b,
  const unsigned int min_transfer_seconds
);

void MergeIntoSchedule(const Schedule& src, Schedule &dest);
