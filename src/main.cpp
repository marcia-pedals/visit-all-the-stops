#include <iostream>
#include <numeric>

#include "Config.h"
#include "MultiSegment.h"
#include "World.h"
#include "Solver.h"
#include <unordered_set>

#include "absl/strings/str_cat.h"
#include "absl/time/civil_time.h"

#include <toml++/toml.h>
#include <absl/flags/parse.h>

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

  Solve(config.world, config.start_stop_id, config.target_stop_ids);

  return 0;
}
