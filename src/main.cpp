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
  std::optional<std::string> err_opt = readConfig(positional[1], config);
  if (err_opt.has_value()) {
    std::cerr << err_opt.value() << "\n";
    return 1;
  }

  std::map<std::string, size_t> segment_count;
  for (const WorldSegment& segment : config.world.segments) {
    segment_count[segment.origin_stop_id]++;
    segment_count[segment.destination_stop_id]++;
  }
  for (const auto [stop_id, count] : segment_count) {
    std::cout << absl::StreamFormat(
      "%-30s %-40s %d\n", stop_id, config.world.stops.at(stop_id).name, count
    );
  }


  // std::vector<std::string> bart_segment_stop_ids = {
  //   "MLBR",
  //   "SFIA",
  //   "DALY",
  //   "RICH",
  //   "ANTC",
  //   "COLS",
  //   "DUBL",
  //   "BERY",
  //   "BALB",
  //   "WOAK",
  //   "LAKE",
  //   "12TH",
  //   "19TH",
  //   "MCAR",
  //   "BAYF",
  //   "FTVL",

  //   // "BERY",
  //   // "BAYF",
  //   // "DUBL",
  //   // "COLS",
  // };
  // std::unordered_set<std::string> prefixed_bart_segment_stop_ids;
  // for (const std::string& stop_id : bart_segment_stop_ids) {
  //   prefixed_bart_segment_stop_ids.insert("bart-place_" + stop_id);
  // }
  // prefixed_bart_segment_stop_ids.insert("bart-OAKL");
  // const auto err_opt = readGTFSToWorld("data/fetched-2024-06-08/bart", "bart-", date, prefixed_bart_segment_stop_ids, world);
  // if (err_opt.has_value()) {
  //   std::cout << err_opt.value() << "\n";
  //   return 1;
  // }

  // std::string prettyRoutes;
  // world.prettyRoutes(prettyRoutes);
  // std::cout << prettyRoutes << "\n";

  // std::string prettyDepartureTable;
  // for (const char* line : {"Red-N", "Red-S", "Yellow-N", "Yellow-S", "Blue-N", "Blue-S", "Green-N", "Green-S"}) {
  //   world.prettyDepartureTable("bart-place_BALB", line, prettyDepartureTable);
  //   absl::StrAppend(&prettyDepartureTable, "\n");
  // }
  // std::cout << prettyDepartureTable << "\n";

  // std::vector<std::string> caltrain_segment_stop_ids = {
  //   "place_MLBR",
  //   "sj_diridon",
  //   "san_francisco",
  // };
  // std::unordered_set<std::string> prefixed_caltrain_segment_stop_ids;
  // for (const std::string& stop_id : caltrain_segment_stop_ids) {
  //   prefixed_caltrain_segment_stop_ids.insert("caltrain-" + stop_id);
  // }
  // const auto err_opt2 = readGTFSToWorld("data/fetched-2024-06-08/caltrain", "caltrain-", date, prefixed_caltrain_segment_stop_ids, world);
  // if (err_opt2.has_value()) {
  //   std::cout << err_opt2.value() << "\n";
  //   return 1;
  // }

  // std::string prettyDepartureTable;
  // for (const char* line : {"L1", "L3", "L4", "L5", "B7"}) {
  //   world.prettyDepartureTable("bart-place_BALB", line, prettyDepartureTable);
  //   absl::StrAppend(&prettyDepartureTable, "\n");
  // }
  // std::cout << prettyDepartureTable << "\n";


  // std::string prettyDepartureTable;
  // world.prettyDepartureTable("caltrain-place_MLBR", std::nullopt, prettyDepartureTable);
  // std::cout << prettyDepartureTable << "\n";

  // std::vector<std::string> vta_segment_stop_ids = {
  //   "PS_MVTC",
  //   "PS_OLDI",
  //   "PS_CHMP",
  //   "PS_TASM",
  //   "PS_BAYP",
  //   "PS_ALUM",
  //   // paseo de san antonio???
  //   "PS_CONV",
  //   "PS_WINC",
  //   "PS_TRSA",  // Santa Teresa
  // };
  // std::unordered_set<std::string> prefixed_vta_segment_stop_ids;
  // for (const std::string& stop_id : vta_segment_stop_ids) {
  //   prefixed_vta_segment_stop_ids.insert("vta-" + stop_id);
  // }
  // const auto err_opt = readGTFSToWorld("data/fetched-2024-06-08/gtfs_vta", "vta-", date, prefixed_vta_segment_stop_ids, world);
  // if (err_opt.has_value()) {
  //   std::cout << err_opt.value() << "\n";
  //   return 1;
  // }

  // Solve(world, "vta-PS_MVTC");

  return 0;
}
