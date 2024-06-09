#include "World.h"

#include <chrono>

#include "csv.hpp"

static std::optional<unsigned int> parseGTFSTime(const std::string& time) {
  if (time.empty()) {
    return std::nullopt;
  }
  // TODO: More error handling!!
  unsigned int hours = std::stoi(time.substr(0, 2));
  unsigned int minutes = std::stoi(time.substr(3, 2));
  unsigned int seconds = std::stoi(time.substr(6, 2));
  return hours * 60 + minutes;
}

static std::chrono::year_month_day parseGTFSDay(const std::string& day) {
  // TODO: Some error handling??
  int year = std::stoi(day.substr(0, 4));
  unsigned int month = std::stoi(day.substr(4, 2));
  unsigned int day_of_month = std::stoi(day.substr(6, 2));
  return std::chrono::year_month_day{std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{day_of_month}};
}

static std::string minutesToHuman(unsigned int minutes) {
  unsigned int hours = minutes / 60;
  minutes %= 60;
  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(2) << hours << ":" << std::setfill('0') << std::setw(2) << minutes;
  return oss.str();
}

static std::string humanRange(const Range& range) {
  if (range.interval == 0) {
    return minutesToHuman(range.start);
  }
  return minutesToHuman(range.start) + " to " + minutesToHuman(range.finish) + " every " + std::to_string(range.interval) + " minutes";
}

// Returns a vector of segments representing the same segments as the input, but hopefully merged into fewer segments.
// Preconditions:
// - All the input segments are singular (they have only one departure each).
// - The origin and destination of all segments are the same.
// - The duration of all segments is the same.
static std::vector<WorldSegment> mergeSegments(const std::vector<WorldSegment>& segments) {
  std::vector<WorldSegment> sorted = segments;
  std::sort(sorted.begin(), sorted.end(), [](const WorldSegment& a, const WorldSegment& b) {
    return a.departures.start < b.departures.start;
  });
  if (sorted.size() <= 1) {
    return sorted;
  }

  std::vector<WorldSegment> result;
  unsigned int interval = sorted[1].departures.start - sorted[0].departures.start;
  unsigned int latest_on_interval_start = sorted[0].departures.start;
  for (unsigned int i = 1; i < sorted.size(); ++i) {
    auto segment = sorted[i];
    if (segment.departures.start == latest_on_interval_start + interval) {
      latest_on_interval_start = segment.departures.start;
    } else {
      result.push_back(segment);
    }
  }
  result = mergeSegments(result);
  result.insert(
    result.begin(),
    WorldSegment{
      Range(sorted[0].departures.start, latest_on_interval_start, interval),
      sorted[0].duration,
      sorted[0].origin_stop_id,
      sorted[0].destination_stop_id,
      sorted[0].route_id
    }
  );

  return result;
}

std::optional<std::string> readServiceIds(
  const std::string& directory,
  const std::string& id_prefix,
  const std::chrono::year_month_day& date,
  std::unordered_set<std::string>& service_ids
) {
  // Figure out the services to load based on the calendars.
  std::string weekday;
  switch (std::chrono::weekday{date}.c_encoding()) {
    case 0:
      weekday = "sunday";
      break;
    case 1:
      weekday = "monday";
      break;
    case 2:
      weekday = "tuesday";
      break;
    case 3:
      weekday = "wednesday";
      break;
    case 4:
      weekday = "thursday";
      break;
    case 5:
      weekday = "friday";
      break;
    case 6:
      weekday = "saturday";
      break;
  }
  csv::CSVReader calendar_reader(directory + "/calendar.txt");
  for (const csv::CSVRow& row : calendar_reader) {
    std::string service_id = id_prefix + row["service_id"].get<>();
    std::string start_date_raw = row["start_date"].get<>();
    std::string end_date_raw = row["end_date"].get<>();
    std::chrono::year_month_day start_date = parseGTFSDay(start_date_raw);
    std::chrono::year_month_day end_date = parseGTFSDay(end_date_raw);
    if (date >= start_date && date <= end_date && row[weekday].get<>() == "1") {
      service_ids.insert(service_id);
    }
  }
  csv::CSVReader calendar_dates_reader(directory + "/calendar_dates.txt");
  for (const csv::CSVRow& row : calendar_dates_reader) {
    std::string service_id = id_prefix + row["service_id"].get<>();
    std::string exception_date_raw = row["date"].get<>();
    std::chrono::year_month_day exception_date = parseGTFSDay(exception_date_raw);
    if (exception_date != date) {
      continue;
    }
    if (row["exception_type"].get<>() == "2") {
      if (service_ids.contains(service_id)) {
        service_ids.erase(service_id);
      }
    } else if (row["exception_type"].get<>() == "1") {
      if (!service_ids.contains(service_id)) {
        service_ids.insert(service_id);
      }
    } else {
      return "Unknown exception type in calendar_dates.txt: " + row["exception_type"].get<>();
    }
  }
  return std::nullopt;
}

std::optional<std::string> readGTFSToWorld(
  const std::string& directory,
  const std::string& id_prefix,
  const std::chrono::year_month_day& date,
  const std::unordered_set<std::string>& segment_stop_ids,
  World& world
) {
  std::unordered_set<std::string> service_ids;
  auto service_ids_err = readServiceIds(directory, id_prefix, date, service_ids);
  if (service_ids_err.has_value()) {
    return service_ids_err;
  }

  // Load routes.
  csv::CSVReader routes_reader(directory + "/routes.txt");
  for (const csv::CSVRow& row : routes_reader) {
    std::string route_id = id_prefix + row["route_id"].get<>();
    std::string route_short_name = row["route_short_name"].get<>();
    world.routes[route_id] = WorldRoute{route_short_name};
  }

  // Load stops.
  csv::CSVReader stops_reader(directory + "/stops.txt");
  for (const csv::CSVRow& row : stops_reader) {
    std::string stop_id = id_prefix + row["stop_id"].get<>();
    std::string stop_name = row["stop_name"].get<>();
    std::string parent_station_raw = row["parent_station"].get<>();
    std::optional<std::string> parent_station = parent_station_raw.empty() ? std::nullopt : std::make_optional(id_prefix + parent_station_raw);
    world.stops[stop_id] = WorldStop{stop_name, parent_station};
  }

  // Validate segment_stop_ids.
  std::vector<const std::string> stops_not_found;
  std::vector<const std::string> stops_not_root;
  for (const std::string& stop_id : segment_stop_ids) {
    auto it = world.stops.find(stop_id);
    if (it == world.stops.end()) {
      stops_not_found.push_back(stop_id);
    } else if (it->second.parent_station_id.has_value()) {
      stops_not_root.push_back(stop_id);
    }
  }
  if (!stops_not_found.empty() || !stops_not_root.empty()) {
    std::string error_message = "Problems reading GTFS:\n";
    for (const std::string& stop_id : stops_not_found) {
      error_message += "  Stop not found: " + stop_id + "\n";
    }
    for (const std::string& stop_id : stops_not_root) {
      error_message += "  Stop not root: " + stop_id + "\n";
    }
    return error_message;
  }

  // Load trips.
  csv::CSVReader trips_reader(directory + "/trips.txt");
  for (const csv::CSVRow& row : trips_reader) {
    if (!service_ids.contains(id_prefix + row["service_id"].get<>())) {
      continue;
    }
    std::string trip_id = id_prefix + row["trip_id"].get<>();
    std::string route_id = id_prefix + row["route_id"].get<>();
    world.trips[trip_id] = WorldTrip{route_id, {}};
  }

  // Load stop times into the trips.
  csv::CSVReader stop_times_reader(directory + "/stop_times.txt");
  for (const csv::CSVRow& row : stop_times_reader) {
    std::string trip_id = id_prefix + row["trip_id"].get<>();
    std::string stop_id = id_prefix + row["stop_id"].get<>();
    auto trip_it = world.trips.find(trip_id);
    if (trip_it == world.trips.end()) {
      // This is expected because we only include trips from some services.
      continue;
    }
    for (;;) {
      auto stop_it = world.stops.find(stop_id);
      if (stop_it == world.stops.end()) {
        return "While reading stop_times for trip " + trip_id + ", stop not found: " + stop_id;
      }
      if (stop_it->second.parent_station_id.has_value()) {
        stop_id = stop_it->second.parent_station_id.value();
      } else {
        break;
      }
    }
    trip_it->second.stop_times.push_back(
      WorldTripStopTimes{
        stop_id,
        parseGTFSTime(row["arrival_time"].get<>()),
        parseGTFSTime(row["departure_time"].get<>())
      }
    );
  }
  for (auto& entry: world.trips) {
    auto& trip = entry.second;
    std::sort(trip.stop_times.begin(), trip.stop_times.end(), [](const WorldTripStopTimes& a, const WorldTripStopTimes& b) {
      return a.departure_time.value_or(0) < b.departure_time.value_or(0);
    });
    bool sorted = std::is_sorted(trip.stop_times.begin(), trip.stop_times.end(), [](const WorldTripStopTimes& a, const WorldTripStopTimes& b) {
      return a.arrival_time.value_or(0) < b.arrival_time.value_or(0);
    });
    if (!sorted) {
      return "Trip " + entry.first + " has wacky stop times.";
    }
  }

  // Segment the trips.

  // First build a map from (origin, destination, route, duration) tuple to "singular" segments.
  std::map<std::tuple<std::string, std::string, std::string, unsigned int>, std::vector<WorldSegment>> singular_segments;
  for (const auto& entry : world.trips) {
    const auto& trip = entry.second;

    std::optional<WorldTripStopTimes> prev;
    for (const auto& stop_time : trip.stop_times) {
      if (!segment_stop_ids.contains(stop_time.stop_id)) {
        continue;
      }
      if (!prev.has_value()) {
        prev = stop_time;
        continue;
      }

      std::string origin_stop_id = prev->stop_id;
      std::string destination_stop_id = stop_time.stop_id;
      assert(prev->departure_time.has_value());
      unsigned int departure_time = prev->departure_time.value();
      assert(stop_time.arrival_time.has_value());
      unsigned int arrival_time = stop_time.arrival_time.value();
      unsigned int duration = arrival_time - departure_time;
      singular_segments[std::make_tuple(origin_stop_id, destination_stop_id, trip.route_id, duration)].push_back(
        WorldSegment{
          Range(departure_time, departure_time, 0),
          duration,
          origin_stop_id,
          destination_stop_id,
          trip.route_id
        }
      );

      prev = stop_time;
    }
  }
  
  // Then try to merge singular segments.
  for (const auto& entry : singular_segments) {
    const auto& segments = entry.second;
    const auto merged = mergeSegments(segments);
    world.segments.insert(world.segments.end(), merged.begin(), merged.end());
  }

  return std::nullopt;
}

void printStops(std::ostream& os, const World& world) {
  for (const auto& entry: world.stops) {
    const auto& stop = entry.second;
    os << entry.first << ": " << stop.name << " (" << stop.parent_station_id.value_or("") << ")\n";
  }
}

void printRoutes(std::ostream& os, const World& world) {
  for (const auto& entry: world.routes) {
    const auto& route = entry.second;
    os << entry.first << ": " << route.name << "\n";
  }
}

void printDepartureTable(std::ostream& os, const World& world, const std::string& stop_id) {
  os << "Departure table for " << world.stops.at(stop_id).name << "\n";
  std::map<std::string, std::vector<WorldSegment>> segments_by_route;
  for (const auto& segment: world.segments) {
    if (segment.origin_stop_id == stop_id) {
      segments_by_route[segment.route_id].push_back(segment);
    }
  }
  for (auto& entry : segments_by_route) {
    const auto& route_id = entry.first;
    auto& segments = entry.second;
    std::sort(segments.begin(), segments.end(), [](const WorldSegment& a, const WorldSegment& b) {
      return a.departures.start < b.departures.start;
    });
    os << "  " << world.routes.at(route_id).name << ":\n";
    for (const auto& segment: segments) {
      os << "    "
        << std::left << std::setfill(' ') << std::setw(40) << humanRange(segment.departures)
        << " (arr " << world.stops.at(segment.destination_stop_id).name << " in " << segment.duration << " mins)\n";
    }
  }
}