#include <iostream>
#include <numeric>

#include "Config.h"
#include "World.h"
#include "Problem.h"
#include "Simplifier.h"
#include <unordered_set>

#include "absl/strings/str_cat.h"
#include "absl/time/civil_time.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"

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
    {.IgnoreSegmentStopIds = true},
    config
  );
  if (err_opt.has_value()) {
    std::cerr << err_opt.value() << "\n";
    return 1;
  }

  AddWalkingSegments(config.world);
  std::cout << "added walking segments\n";

  Problem problem = BuildProblem(config.world);
  std::cout << "built\n";

  problem = SimplifyProblem(problem, config.target_stop_ids);
  std::cout << "simplified\n";

  std::ofstream ser_of("problem.json");
  {
    cereal::JSONOutputArchive archive(ser_of);
    archive(problem);
  }
  std::cout << "dumped\n";

  nlohmann::json result;

  double minLat = 1000;
  double maxLat = -1000;
  double minLon = 1000;
  double maxLon = -1000;

  nlohmann::json& result_vertices = result["vertices"];
  for (const std::string& stop_id : problem.stop_index_to_id) {
    nlohmann::json& vertex = result_vertices[stop_id];
    const WorldStop& stop = config.world.stops.at(stop_id);
    vertex["lat"] = stop.lat;
    vertex["lon"] = stop.lon;
    vertex["name"] = stop.name;

    double lat = std::atof(stop.lat.c_str());
    double lon = std::atof(stop.lon.c_str());
    minLat = std::min(minLat, lat);
    maxLat = std::max(maxLat, lat);
    minLon = std::min(minLon, lon);
    maxLon = std::max(maxLon, lon);
  }

  nlohmann::json& bounds = result["bounds"];
  bounds["min_lat"] = minLat;
  bounds["max_lat"] = maxLat;
  bounds["min_lon"] = minLon;
  bounds["max_lon"] = maxLon;

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
        for (const size_t trip_index : seg.trip_indices) {
          result_segment["trips"].push_back(problem.trip_index_to_id[trip_index]);
        }
      }
    }
  }


  std::cout << result.dump(2) << "\n";

  return 0;
}
