#pragma once

#include <algorithm>
#include <chrono>
#include <map>
#include <unordered_set>
#include <optional>
#include <string>
#include <vector>

struct WorldRoute {
  std::string name;
};

struct WorldStop {
  std::string name;
  std::optional<std::string> parent_station_id;
};

struct WorldSegment {
  unsigned int departure_time;
  unsigned int duration;

  std::string origin_stop_id;
  std::string destination_stop_id;
  std::string route_id;
};

struct WorldTripStopTimes {
  std::string stop_id;
  std::optional<unsigned int> arrival_time;
  std::optional<unsigned int> departure_time;
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
};

std::optional<std::string> readServiceIds(
  const std::string& directory,
  const std::string& id_prefix,
  const std::chrono::year_month_day& date,
  std::unordered_set<std::string>& service_ids
);

// Adds the GTFS data at `directory` to `world`.
//
// All GTFS IDs are prefixed with `id_prefix`.
//
// Trips are segmented at the stops in `segment_stop_ids`, which must be root stops (i.e. stops with
// no parent station). Trips are always interpreted as stopping at the root ancestor of the stop
// specified in the trip.
//
// Returns an error message if something went wrong, otherwise returns nullopt.
std::optional<std::string> readGTFSToWorld(
  const std::string& directory,
  const std::string& id_prefix,
  const std::chrono::year_month_day& date,
  const std::unordered_set<std::string>& segment_stop_ids,
  World& world
);

void printStops(std::ostream& os, const World& world);
void printRoutes(std::ostream& os, const World& world);
void printDepartureTable(std::ostream& os, const World& world, const std::string& stop_id);