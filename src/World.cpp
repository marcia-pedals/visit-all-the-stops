#include "World.h"

#include <sstream>
#include <variant>

#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/numbers.h"
#include "absl/time/civil_time.h"

#include "csv.hpp"

// Parses an optional GTFS time, which is a string of the form "HH:MM:SS" or "".
static std::variant<std::optional<WorldTime>, std::string> parseGTFSTime(absl::string_view time) {
  if (time.empty()) {
    return std::nullopt;
  }
  std::vector<absl::string_view> parts = absl::StrSplit(time, ':');
  if (parts.size() != 3) {
    return absl::StrCat("Invalid time: ", time);
  }
  bool success = true;
  unsigned int hours = 0, minutes = 0, seconds = 0;
  success &= absl::SimpleAtoi(parts[0], &hours);
  success &= absl::SimpleAtoi(parts[1], &minutes);
  success &= absl::SimpleAtoi(parts[2], &seconds);
  if (!success) {
    return absl::StrCat("Invalid time: ", time);
  }
  return WorldTime(hours * 3600 + minutes * 60 + seconds);
}

// Parses a required GTFS day, which is a string of the form "YYYYMMDD".
static std::variant<absl::CivilDay, std::string> parseGTFSDay(absl::string_view day) {
  if (day.size() != 8) {
    return absl::StrCat("Invalid day: ", day);
  }
  unsigned int year = 0, month = 0, day_of_month = 0;
  bool success = true;
  success &= absl::SimpleAtoi(day.substr(0, 4), &year);
  success &= absl::SimpleAtoi(day.substr(4, 2), &month);
  success &= absl::SimpleAtoi(day.substr(6, 2), &day_of_month);
  if (!success) {
    return absl::StrCat("Invalid day: ", day);
  }
  return absl::CivilDay(year, month, day_of_month);
}

std::optional<std::string> readServiceIds(
  const std::string& directory,
  const std::string& id_prefix,
  absl::CivilDay date,
  std::unordered_set<std::string>& service_ids
) {
  // Figure out the services to load based on the calendars.
  std::ostringstream weekday_stream;
  weekday_stream << absl::GetWeekday(date);
  std::string weekday = absl::AsciiStrToLower(weekday_stream.str());
  csv::CSVReader calendar_reader(directory + "/calendar.txt");
  for (const csv::CSVRow& row : calendar_reader) {
    std::string service_id = id_prefix + row["service_id"].get<>();
    std::string start_date_raw = row["start_date"].get<>();
    std::string end_date_raw = row["end_date"].get<>();
    auto start_date_or = parseGTFSDay(start_date_raw);
    if (std::holds_alternative<std::string>(start_date_or)) {
      return std::get<1>(start_date_or);
    }
    absl::CivilDay start_date = std::get<0>(start_date_or);
    auto end_date_or = parseGTFSDay(end_date_raw);
    if (std::holds_alternative<std::string>(end_date_or)) {
      return std::get<1>(end_date_or);
    }
    absl::CivilDay end_date = std::get<0>(end_date_or);
    if (date >= start_date && date <= end_date && row[weekday].get<>() == "1") {
      service_ids.insert(service_id);
    }
  }
  csv::CSVReader calendar_dates_reader(directory + "/calendar_dates.txt");
  for (const csv::CSVRow& row : calendar_dates_reader) {
    std::string service_id = id_prefix + row["service_id"].get<>();
    std::string exception_date_raw = row["date"].get<>();
    auto exception_date_or = parseGTFSDay(exception_date_raw);
    if (std::holds_alternative<std::string>(exception_date_or)) {
      return std::get<1>(exception_date_or);
    }
    absl::CivilDay exception_date = std::get<0>(exception_date_or);
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
  absl::CivilDay date,
  const std::unordered_set<std::string>* segment_stop_ids,
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
  if (segment_stop_ids != nullptr) {
    std::vector<const std::string> stops_not_found;
    std::vector<const std::string> stops_not_root;
    for (const std::string& stop_id : *segment_stop_ids) {
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
    auto arrival_time = parseGTFSTime(row["arrival_time"].get<>());
    if (std::holds_alternative<std::string>(arrival_time)) {
      return std::get<1>(arrival_time);
    }
    auto departure_time = parseGTFSTime(row["departure_time"].get<>());
    if (std::holds_alternative<std::string>(departure_time)) {
      return std::get<1>(departure_time);
    }
    trip_it->second.stop_times.push_back(
      WorldTripStopTimes{
        stop_id,
        std::get<0>(arrival_time),
        std::get<0>(departure_time)
      }
    );
  }

  // Error checking on the stop times.
  for (auto& entry: world.trips) {
    auto& trip = entry.second;
    std::sort(trip.stop_times.begin(), trip.stop_times.end(), [](const WorldTripStopTimes& a, const WorldTripStopTimes& b) {
      return a.departure_time.value_or(WorldTime(0)).seconds < b.departure_time.value_or(WorldTime(0)).seconds;
    });
    bool sorted = std::is_sorted(trip.stop_times.begin(), trip.stop_times.end(), [](const WorldTripStopTimes& a, const WorldTripStopTimes& b) {
      return a.arrival_time.value_or(WorldTime(0)).seconds < b.arrival_time.value_or(WorldTime(0)).seconds;
    });
    if (!sorted) {
      return "Trip " + entry.first + " has wacky stop times.";
    }
  }

  // Segment the trips.
  for (const auto& entry : world.trips) {
    const auto& trip = entry.second;

    std::optional<WorldTripStopTimes> prev;
    for (const auto& stop_time : trip.stop_times) {
      if (segment_stop_ids != nullptr && !segment_stop_ids->contains(stop_time.stop_id)) {
        continue;
      }
      if (!prev.has_value()) {
        prev = stop_time;
        continue;
      }

      std::string origin_stop_id = prev->stop_id;
      std::string destination_stop_id = stop_time.stop_id;
      assert(prev->departure_time.has_value());
      WorldTime departure_time = prev->departure_time.value();
      assert(stop_time.arrival_time.has_value());
      WorldTime arrival_time = stop_time.arrival_time.value();
      WorldDuration duration = WorldDuration(arrival_time.seconds - departure_time.seconds);
      world.segments.push_back(WorldSegment{
        .departure_time = departure_time,
        .duration = duration,
        .origin_stop_id = origin_stop_id,
        .destination_stop_id = destination_stop_id,
        .route_id = trip.route_id
      });

      prev = stop_time;
    }
  }
  std::sort(world.segments.begin(), world.segments.end(), [](const WorldSegment& a, const WorldSegment& b) {
    if (a.departure_time.seconds == b.departure_time.seconds) {
      return a.duration.seconds < b.duration.seconds;
    }
    return a.departure_time.seconds < b.departure_time.seconds;
  });
  
  return std::nullopt;
}

void World::prettyRoutes(std::string& result) const {
  for (const auto& entry: routes) {
    const auto& route = entry.second;
    absl::StrAppend(&result, entry.first, ": ", route.name, "\n");
  }
}

void World::prettyDepartureTable(const std::string& stop_id, const std::optional<std::string>& line_name, std::string& result) const {
  constexpr const char* table_format = "%-10s %-15s %-40s %s\n";
  absl::StrAppend(&result, "Departure table for ", stops.at(stop_id).name, "\n");
  absl::StrAppendFormat(&result, table_format, "Time", "Line", "Next Stop", "Next Stop Time");
  for (const auto& segment : segments) {
    if (segment.origin_stop_id != stop_id || (line_name.has_value() && routes.at(segment.route_id).name != line_name)) {
      continue;
    }
    absl::StrAppendFormat(
      &result,
      table_format,
      absl::StrCat(segment.departure_time, ""), // TODO: Figure out how to use formatter.
      routes.at(segment.route_id).name,
      stops.at(segment.destination_stop_id).name,
      absl::StrCat(WorldTime(segment.departure_time.seconds + segment.duration.seconds), "") // TODO: Figure out how to use formatter.
    );
  }
}

// Some leftover code from when I was using MultiSegments.

// static std::string humanRange(const Range& range) {
//   if (range.interval == 0) {
//     return minutesToHuman(range.start);
//   }
//   return minutesToHuman(range.start) + " to " + minutesToHuman(range.finish) + " every " + std::to_string(range.interval) + " minutes";
// }

// // Returns a vector of segments representing the same segments as the input, but hopefully merged into fewer segments.
// // Preconditions:
// // - All the input segments are singular (they have only one departure each).
// // - The origin and destination of all segments are the same.
// // - The duration of all segments is the same.
// static std::vector<WorldSegment> mergeSegments(const std::vector<WorldSegment>& segments) {
//   std::vector<WorldSegment> sorted = segments;
//   std::sort(sorted.begin(), sorted.end(), [](const WorldSegment& a, const WorldSegment& b) {
//     return a.departures.start < b.departures.start;
//   });
//   if (sorted.size() <= 1) {
//     return sorted;
//   }

//   std::vector<WorldSegment> result;
//   unsigned int interval = sorted[1].departures.start - sorted[0].departures.start;
//   unsigned int latest_on_interval_start = sorted[0].departures.start;
//   for (unsigned int i = 1; i < sorted.size(); ++i) {
//     auto segment = sorted[i];
//     if (segment.departures.start == latest_on_interval_start + interval) {
//       latest_on_interval_start = segment.departures.start;
//     } else {
//       result.push_back(segment);
//     }
//   }
//   result = mergeSegments(result);
//   result.insert(
//     result.begin(),
//     WorldSegment{
//       Range(sorted[0].departures.start, latest_on_interval_start, interval),
//       sorted[0].duration,
//       sorted[0].origin_stop_id,
//       sorted[0].destination_stop_id,
//       sorted[0].route_id
//     }
//   );

//   return result;
// }
