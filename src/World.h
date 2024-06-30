#pragma once

#include <algorithm>
#include <map>
#include <unordered_set>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/time/civil_time.h"

struct WorldTime {
  // Seconds since beginning of service day.
  unsigned int seconds;

  WorldTime(unsigned int seconds) : seconds(seconds) {}

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const WorldTime& time) {
    // Note: Drops seconds.
    absl::Format(&sink, "%02u:%02u", time.seconds / 3600, (time.seconds / 60) % 60);
  }
};

struct WorldDuration {
  unsigned int seconds;

  WorldDuration(unsigned int seconds) : seconds(seconds) {}

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const WorldDuration& duration) {
    // Note: Drops seconds.
    if (duration.seconds >= 3600) {
      absl::Format(&sink, "%02uh%02um", duration.seconds / 3600, (duration.seconds / 60) % 60);
    } else {
      absl::Format(&sink, "%02um", duration.seconds / 60);
    }
  }
};

struct WorldRoute {
  std::string name;
};

struct WorldStop {
  std::string name;
  std::optional<std::string> parent_station_id;
};

struct WorldSegment {
  WorldTime departure_time;
  WorldDuration duration;

  std::string origin_stop_id;
  std::string destination_stop_id;
  std::string route_id;
};

struct WorldTripStopTimes {
  std::string stop_id;
  std::optional<WorldTime> arrival_time;
  std::optional<WorldTime> departure_time;
};

struct WorldTrip {
  std::string route_id;
  std::vector<WorldTripStopTimes> stop_times;
};

struct World {
  std::map<std::string, WorldRoute> routes;
  std::map<std::string, WorldStop> stops;
  std::map<std::string, WorldTrip> trips;
  std::vector<WorldSegment> segments;

  void prettyRoutes(std::string& result) const;
  void prettyDepartureTable(const std::string& stop_id, const std::optional<std::string>& line_name, std::string& result) const;
};

std::optional<std::string> readServiceIds(
  const std::string& directory,
  const std::string& id_prefix,
  absl::CivilDay date,
  std::unordered_set<std::string>& service_ids
);

// Adds the GTFS data at `directory` to `world`.
//
// All GTFS IDs are prefixed with `id_prefix`.
//
// If `segment_stop_ids` has a value, trips are segmented at the stops in `segment_stop_ids`, which
// must be root stops (i.e. stops with no parent station). Otherwise, trips are segmented at all
// stops.
//
// Trips are always interpreted as stopping at the root ancestor of the stop specified in the trip.
//
// Returns an error message if something went wrong, otherwise returns nullopt.
std::optional<std::string> readGTFSToWorld(
  const std::string& directory,
  const std::string& id_prefix,
  absl::CivilDay date,
  const std::unordered_set<std::string>* segment_stop_ids,
  World& world
);
