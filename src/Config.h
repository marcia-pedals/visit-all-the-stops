#pragma once

#include <optional>

#include "World.h"

struct ReadConfigOptions {
  bool IgnoreSegmentStopIds;
};

struct Config {
  World world;
  std::vector<std::string> target_stop_ids;
};

// Returns an error message on failure.
std::optional<std::string> readConfig(
  absl::string_view config_file, const ReadConfigOptions& options, Config& config
);
