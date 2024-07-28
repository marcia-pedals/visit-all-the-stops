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
    result_vertices.push_back({});
    nlohmann::json& vertex = result_vertices.back();
    vertex["id"] = stop_id;
    const WorldStop& stop = config.world.stops.at(stop_id);
    vertex["lat"] = stop.lat;
    vertex["lon"] = stop.lon;
    vertex["name"] = stop.name;
  }

  std::cout << result.dump(2) << "\n";

  return 0;
}
