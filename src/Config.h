#pragma once

#include <optional>

#include "World.h"

struct Config {
  World world;
};

// Returns an error message on failure.
std::optional<std::string> readConfig(absl::string_view config_file, Config& config);
