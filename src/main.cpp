#include <iostream>
#include <numeric>

#include "MultiSegment.h"
#include "World.h"
#include <unordered_set>

int main() {
  World world;

  std::vector<std::string> bart_segment_stop_ids = {
    "MLBR",
    "WDUB",
    "ANTC",
    "RICH",
    "BERY",
  };
  std::unordered_set<std::string> prefixed_bart_segment_stop_ids;
  for (const std::string& stop_id : bart_segment_stop_ids) {
    prefixed_bart_segment_stop_ids.insert("bart-place_" + stop_id);
  }
  const auto err_opt = readGTFSToWorld("data/fetched-2024-06-08/bart", "bart-", prefixed_bart_segment_stop_ids, world);
  if (err_opt.has_value()) {
    std::cout << err_opt.value() << "\n";
    return 1;
  }
  // printStops(std::cout, world);
  // printRoutes(std::cout, world);
  printSegmentsFrom(std::cout, world, "bart-place_MLBR");
}
