#include "Config.h"

#include <iostream>

#include "absl/strings/str_cat.h"
#include <toml++/toml.h>

std::optional<std::string> readConfig(
  absl::string_view config_file, const ReadConfigOptions& options, Config& config
) {
  toml::table config_table;
  try {
    config_table = toml::parse_file(config_file);
  } catch (const std::exception& e) {
    return absl::StrCat("Error parsing config: ", e.what());
  }

  if (config_table.get("target_stop_ids") != nullptr) {
    const toml::array* target_stop_ids_arr = config_table.at("target_stop_ids").as_array();
    for (size_t i = 0; i < target_stop_ids_arr->size(); ++i) {
      config.target_stop_ids.push_back(target_stop_ids_arr->at(i).as_string()->get());
    }
  }

  absl::CivilDay date;
  const std::string& date_str = config_table.at("date").as_string()->get();
  if (!absl::ParseCivilTime(date_str, &date)) {
    return absl::StrCat("Error parsing date ", date_str);
  }

  const toml::array* gtfs_arr = config_table.at("gtfs").as_array();
  for (size_t i = 0; i < gtfs_arr->size(); ++i) {
    const toml::table* gtfs_el = gtfs_arr->at(i).as_table();
    const std::string& dir = gtfs_el->at("dir").as_string()->get();
    const std::string& prefix = gtfs_el->at("prefix").as_string()->get();

    std::unordered_set<std::string> segment_stop_ids;
    if (gtfs_el->get("segment_stop_ids") != nullptr) {
      const toml::array* segment_stop_ids_array = gtfs_el->at("segment_stop_ids").as_array();
      for (size_t j = 0; j < segment_stop_ids_array->size(); ++j) {
        segment_stop_ids.insert(segment_stop_ids_array->at(j).as_string()->get());
      }
    }

    const auto err_opt = readGTFSToWorld(
      dir, prefix, date, options.IgnoreSegmentStopIds ? nullptr : &segment_stop_ids, config.world
    );
    if (err_opt.has_value()) {
      return err_opt.value();
    }
  }

  if (config_table.get("anytime_connections") != nullptr) {
    const toml::array* anytime_arr = config_table.at("anytime_connections").as_array();
    for (size_t i = 0; i < anytime_arr->size(); ++i) {
      const toml::table* anytime_el = anytime_arr->at(i).as_table();
      WorldAnytimeConnection connection;
      connection.origin_stop_id = anytime_el->at("origin").as_string()->get();
      connection.destination_stop_id = anytime_el->at("destination").as_string()->get();
      connection.duration = WorldDuration(anytime_el->at("duration_min").as_integer()->get() * 60);
      config.world.anytime_connections.push_back(connection);

      // Also add the reverse connection!!
      std::swap(connection.origin_stop_id, connection.destination_stop_id);
      config.world.anytime_connections.push_back(connection);
    }
  }

  return std::nullopt;
}
