
#include <iostream>
#include <optional>
#include <string>
#include <set>

#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/strings/str_join.h>

#include "Config.h"

ABSL_FLAG(bool, show_routes, true, "Show routes at each stop");
ABSL_FLAG(std::vector<std::string>, route_filter, {}, "Only show stops on these routes");

int main(int argc, char* argv[]) {
  std::vector<char*> positional = absl::ParseCommandLine(argc, argv);
  if (positional.size() != 2) {
    std::cerr << "Usage: " << positional[0] << " <config.toml>\n";
    return 1;
  }

  const std::vector<std::string>& route_filter = absl::GetFlag(FLAGS_route_filter);

  Config config;
  std::optional<std::string> err_opt = readConfig(positional[1], config);
  if (err_opt.has_value()) {
    std::cerr << err_opt.value() << "\n";
    return 1;
  }

  std::map<std::string, size_t> segment_count;
  std::map<std::string, std::set<std::string>> routes;
  for (const WorldSegment& segment : config.world.segments) {
    segment_count[segment.origin_stop_id]++;
    segment_count[segment.destination_stop_id]++;
    routes[segment.origin_stop_id].insert(segment.route_id);
    routes[segment.destination_stop_id].insert(segment.route_id);
  }
  for (const auto [stop_id, count] : segment_count) {
    if (route_filter.size() > 0) {
      bool found = false;
      for (const std::string& route_id : route_filter) {
        if (routes[stop_id].contains(route_id)) {
          found = true;
          break;
        }
      }
      if (!found) {
        continue;
      }
    }
    std::cout << absl::StreamFormat(
      "%-30s %-40s %d\n", stop_id, config.world.stops.at(stop_id).name, count
    );
    if (absl::GetFlag(FLAGS_show_routes)) {
      for (const std::string& route_id : routes[stop_id]) {
        std::cout << "  " << route_id << "\n";
      }
    }
  }

  return 0;
}
