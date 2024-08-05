#include <iostream>
#include <numeric>

#include "Config.h"
#include "World.h"
#include "Problem.h"
#include <unordered_set>

#include "absl/strings/str_cat.h"
#include "absl/time/civil_time.h"

#include <toml++/toml.h>
#include <absl/flags/parse.h>
#include <nlohmann/json.hpp>

int main(int argc, char* argv[]) {
  std::vector<char*> positional = absl::ParseCommandLine(argc, argv);
  if (positional.size() != 2) {
    std::cerr << "Usage: " << positional[0] << " <config.toml>\n";
    return 1;
  }

  Config config;
  std::optional<std::string> err_opt = readConfig(
    positional[1],
    {.IgnoreSegmentStopIds = false},
    config
  );
  if (err_opt.has_value()) {
    std::cerr << err_opt.value() << "\n";
    return 1;
  }

  Problem problem = BuildProblem(config.world);

  nlohmann::json result;

  nlohmann::json& result_vertices = result["vertices"];
  for (const std::string& stop_id : problem.stop_index_to_id) {
    nlohmann::json& vertex = result_vertices[stop_id];
    const WorldStop& stop = config.world.stops.at(stop_id);
    vertex["lat"] = stop.lat;
    vertex["lon"] = stop.lon;
    vertex["name"] = stop.name;
  }

  nlohmann::json& result_edges = result["edges"];
  for (size_t origin_stop_index = 0; origin_stop_index < problem.edges.size(); ++origin_stop_index) {
    const std::string& origin_stop_id = problem.stop_index_to_id[origin_stop_index];
    for (const Edge& edge : problem.edges[origin_stop_index]) {
      const size_t destination_stop_index = edge.destination_stop_index;
      const std::string& destination_stop_id = problem.stop_index_to_id[destination_stop_index];
  
      result_edges.push_back({});
      nlohmann::json& result_edge = result_edges.back();
      result_edge["origin_id"] = origin_stop_id;
      result_edge["destination_id"] = destination_stop_id;

      nlohmann::json& result_segments = result_edge["segments"];
      for (const Segment& seg : edge.schedule.segments) {
        result_segments.push_back({});
        nlohmann::json& result_segment = result_segments.back();
        result_segment["departure_time"] = absl::StrCat(seg.departure_time);
        result_segment["arrival_time"] = absl::StrCat(seg.arrival_time);
        // TODO: Could include some trip identifier(s).
      }
    }
  }


  std::cout << result.dump(2) << "\n";

  return 0;
}
