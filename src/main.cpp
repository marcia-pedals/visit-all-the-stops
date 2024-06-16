#include <iostream>
#include <numeric>

#include "MultiSegment.h"
#include "World.h"
#include <unordered_set>

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

  std::string prettyRoutes;
  world.prettyRoutes(prettyRoutes);
  std::cout << prettyRoutes << "\n";

  std::string prettyDepartureTable;
  world.prettyDepartureTable("bart-place_BALB", "Red-N", prettyDepartureTable);
  std::cout << prettyDepartureTable << "\n";

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

  // prettyDepartureTable = "";
  // world.prettyDepartureTable("caltrain-place_MLBR", std::nullopt, prettyDepartureTable);
  // std::cout << prettyDepartureTable << "\n";

  // for (const std::string& stop_id : prefixed_bart_segment_stop_ids) {
  //   printDepartureTable(std::cout, world, stop_id);
  //   std::cout << "\n";
  // }

  // for (const std::string& stop_id : prefixed_caltrain_segment_stop_ids) {
  //   printDepartureTable(std::cout, world, stop_id);
  //   std::cout << "\n";
  // }

  return 0;

  // Next steps:
  // Unmerge segments that only have 2 departures because those are useless.
  // Try to fetch up-to-date Caltrain GTFS.
  // Add tests of all the departure tables.
  //
  // Think about merging connections during the search or somehow change the Range to be
  // "approximately regular" so that we don't get split up results for Caltrain segments that are
  // like 1-2 minutes different from each other.
  }
