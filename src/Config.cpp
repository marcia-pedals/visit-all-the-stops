#include "Config.h"

#include <iostream>

#include "absl/strings/str_cat.h"
#include <toml++/toml.h>

std::optional<std::string> readConfig(absl::string_view config_file, Config& config) {
  std::cout << "Reading config: " << config_file << "\n";
  toml::table config_table;
  try {
    config_table = toml::parse_file(config_file);
  } catch (const std::exception& e) {
    return absl::StrCat("Error parsing config: ", e.what());
  }

  absl::CivilDay date;
  const std::string& date_str = config_table.at("date").as_string()->get();
  if (!absl::ParseCivilTime(date_str, &date)) {
    return absl::StrCat("Error parsing date ", date_str);
  }

  const auto* gtfs_arr = config_table.at("gtfs").as_array();
  for (size_t i = 0; i < gtfs_arr->size(); ++i) {
    const auto& gtfs_el = gtfs_arr->at(i);
    const std::string& dir = gtfs_el.as_table()->get("dir")->as_string()->get();
    const std::string& prefix = gtfs_el.as_table()->get("prefix")->as_string()->get();
    const auto err_opt = readGTFSToWorld(dir, prefix, date, nullptr, config.world);
    if (err_opt.has_value()) {
      return err_opt.value();
    }
  }

  return std::nullopt;
}
