#include <iostream>
#include <numeric>

#include "MultiSegment.h"
#include "World.h"
#include <unordered_set>

#include "absl/strings/str_cat.h"
#include "absl/time/civil_time.h"

int main() {
  absl::CivilDay date(2024, 5, 28);
  World world;

  std::vector<std::string> bart_segment_stop_ids = {
    "MLBR",
    "WDUB",
    "ANTC",
    "RICH",
    "BERY",
    "BALB",
    "DALY",
  };
  std::unordered_set<std::string> prefixed_bart_segment_stop_ids;
  for (const std::string& stop_id : bart_segment_stop_ids) {
    prefixed_bart_segment_stop_ids.insert("bart-place_" + stop_id);
  }
  const auto err_opt = readGTFSToWorld("data/fetched-2024-06-08/bart", "bart-", date, prefixed_bart_segment_stop_ids, world);
  if (err_opt.has_value()) {
    std::cout << err_opt.value() << "\n";
    return 1;
  }

  // std::string prettyRoutes;
  // world.prettyRoutes(prettyRoutes);
  // std::cout << prettyRoutes << "\n";

  // std::string prettyDepartureTable;
  // for (const char* line : {"Red-N", "Red-S", "Yellow-N", "Yellow-S", "Blue-N", "Blue-S", "Green-N", "Green-S"}) {
  //   world.prettyDepartureTable("bart-place_BALB", line, prettyDepartureTable);
  //   absl::StrAppend(&prettyDepartureTable, "\n");
  // }
  // std::cout << prettyDepartureTable << "\n";

  std::vector<std::string> caltrain_segment_stop_ids = {
    "place_MLBR",
    "sj_diridon",
    "san_francisco",
  };
  std::unordered_set<std::string> prefixed_caltrain_segment_stop_ids;
  for (const std::string& stop_id : caltrain_segment_stop_ids) {
    prefixed_caltrain_segment_stop_ids.insert("caltrain-" + stop_id);
  }
  const auto err_opt2 = readGTFSToWorld("data/fetched-2024-06-08/caltrain", "caltrain-", date, prefixed_caltrain_segment_stop_ids, world);
  if (err_opt2.has_value()) {
    std::cout << err_opt2.value() << "\n";
    return 1;
  }

  // std::string prettyDepartureTable;
  // for (const char* line : {"L1", "L3", "L4", "L5", "B7"}) {
  //   world.prettyDepartureTable("bart-place_BALB", line, prettyDepartureTable);
  //   absl::StrAppend(&prettyDepartureTable, "\n");
  // }
  // std::cout << prettyDepartureTable << "\n";


  std::string prettyDepartureTable;
  world.prettyDepartureTable("caltrain-place_MLBR", std::nullopt, prettyDepartureTable);
  std::cout << prettyDepartureTable << "\n";

  return 0;
}
