#include <algorithm>
#include <iterator>
#include <fstream>
#include <sstream>

#include "absl/strings/str_cat.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

#include "World.h"

TEST(
  WorldTest,
  readServiceIdsBartWeekday
) {
  std::unordered_set<std::string> service_ids;
  readServiceIds("data/fetched-2024-06-08/bart", "bart-", absl::CivilDay(2024, 6, 7), service_ids);
  EXPECT_THAT(
    service_ids,
     testing::UnorderedElementsAre(
      "bart-2024_01_15-DX-MVS-Weekday-01",
      "bart-2024_01_15-DX19-Weekday-01",
      "bart-2024_01_15-DX20-Weekday-01"
    )
  );
}

TEST(
  WorldTest,
  readServiceIdsBartSaturday
) {
  std::unordered_set<std::string> service_ids;
  readServiceIds("data/fetched-2024-06-08/bart", "bart-", absl::CivilDay(2024, 6, 8), service_ids);
  EXPECT_THAT(
    service_ids,
    testing::UnorderedElementsAre(
      "bart-2024_01_15-SA-MVS-Saturday-01",
      "bart-2024_01_15-SA19-Saturday-01",
      "bart-2024_01_15-SA20-Saturday-01"
    )
  );
}

TEST(
  WorldTest,
  readServiceIdsBartSunday
) {
  std::unordered_set<std::string> service_ids;
  readServiceIds("data/fetched-2024-06-08/bart", "bart-", absl::CivilDay(2024, 6, 9), service_ids);
  EXPECT_THAT(
    service_ids,
    testing::UnorderedElementsAre(
      "bart-2024_01_15-SU-MVS-Sunday-01",
      "bart-2024_01_15-SU19-Sunday-01",
      "bart-2024_01_15-SU20-Sunday-01"
    )
  );
}

TEST(
  WorldTest,
  readServiceIdsBartFourthOfJuly
) {
  std::unordered_set<std::string> service_ids;
  readServiceIds("data/fetched-2024-06-08/bart", "bart-", absl::CivilDay(2024, 7, 4), service_ids);
  EXPECT_THAT(
    service_ids,
    testing::UnorderedElementsAre(
      "bart-2024_01_15-SU-MVS-Sunday-01",
      "bart-2024_01_15-SU19-Sunday-01",
      "bart-2024_01_15-SU20-Sunday-01"
    )
  );
}

TEST(
  WorldTest,
  readServiceIdsCaltrainWeekday
) {
  std::unordered_set<std::string> service_ids;
  readServiceIds("data/fetched-2024-06-08/caltrain", "caltrain-", absl::CivilDay(2024, 6, 13), service_ids);
  EXPECT_THAT(
    service_ids,
    testing::UnorderedElementsAre("caltrain-72982")
  );
}

TEST(
  WorldTest,
  readServiceIdsCaltrainWeekend
) {
  std::unordered_set<std::string> service_ids;
  readServiceIds("data/fetched-2024-06-08/caltrain", "caltrain-", absl::CivilDay(2024, 6, 16), service_ids);
  EXPECT_THAT(
    service_ids,
    testing::UnorderedElementsAre("caltrain-72981")
  );
}

TEST(
  WorldTest,
  readServiceIdsCaltrainMemorialDay
) {
  std::unordered_set<std::string> service_ids;
  readServiceIds("data/fetched-2024-06-08/caltrain", "caltrain-", absl::CivilDay(2024, 5, 27), service_ids);
  EXPECT_THAT(
    service_ids,
    testing::UnorderedElementsAre("caltrain-72981")
  );
}

TEST(
  WorldTest,
  readServiceIdsCaltrainSpecialElectricTestingShutdown
) {
  std::unordered_set<std::string> service_ids;
  readServiceIds("data/fetched-2024-06-08/caltrain", "caltrain-", absl::CivilDay(2024, 6, 9), service_ids);
  EXPECT_THAT(
    service_ids,
    testing::UnorderedElementsAre()
  );
}

TEST(
  WorldTest,
  bartDepartureTable
) {
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

  World world;
  const auto err_opt = readGTFSToWorld(
    "data/fetched-2024-06-08/bart",
    "bart-",
    absl::CivilDay(2024, 6, 7),
    &prefixed_bart_segment_stop_ids,
    world
  );
  ASSERT_EQ(err_opt, std::nullopt);

  std::string prettyDepartureTable;
  for (const char* line : {"Red-N", "Red-S", "Yellow-N", "Yellow-S", "Blue-N", "Blue-S", "Green-N", "Green-S"}) {
    world.prettyDepartureTable("bart-place_BALB", line, prettyDepartureTable);
    absl::StrAppend(&prettyDepartureTable, "\n");
  }

  std::ifstream expected_file("data/testdata/expected_bart_tables.txt");
  ASSERT_TRUE(expected_file.is_open());
  std::stringstream expected_stream;
  expected_stream << expected_file.rdbuf();
  std::string expected = expected_stream.str();
  EXPECT_EQ(prettyDepartureTable, expected);
}

TEST(
  WorldTest,
  caltrainDepartureTable
) {
  std::vector<std::string> caltrain_segment_stop_ids = {
    "place_MLBR",
    "sj_diridon",
    "san_francisco",
  };
  std::unordered_set<std::string> prefixed_caltrain_segment_stop_ids;
  for (const std::string& stop_id : caltrain_segment_stop_ids) {
    prefixed_caltrain_segment_stop_ids.insert("caltrain-" + stop_id);
  }

  World world;
  const auto err_opt = readGTFSToWorld(
    "data/fetched-2024-06-08/caltrain",
    "caltrain-",
    absl::CivilDay(2024, 6, 7),
    &prefixed_caltrain_segment_stop_ids,
    world
  );
  ASSERT_EQ(err_opt, std::nullopt);

  std::string prettyDepartureTable;
  world.prettyDepartureTable("caltrain-place_MLBR", std::nullopt, prettyDepartureTable);

  std::ifstream expected_file("data/testdata/expected_caltrain_tables.txt");
  ASSERT_TRUE(expected_file.is_open());
  std::stringstream expected_stream;
  expected_stream << expected_file.rdbuf();
  std::string expected = expected_stream.str();
  EXPECT_EQ(prettyDepartureTable, expected);
}

TEST(
  WorldTest,
  bartSFIADuplicateStopTimeDropped
) {
  World world;
  const auto err_opt = readGTFSToWorld(
    "data/fetched-2024-06-08/bart",
    "bart-",
    absl::CivilDay(2024, 6, 7),
    nullptr,
    world
  );
  ASSERT_EQ(err_opt, std::nullopt);

  EXPECT_EQ(
    world.trips["bart-1508826"].stop_times[0],
    (WorldTripStopTimes{
      .stop_id = "bart-place_MLBR",
      .arrival_time = WorldTime(6 * 3600 + 13 * 60),
      .departure_time = WorldTime(6 * 3600 + 13 * 60)
    })
  );
  EXPECT_EQ(
    world.trips["bart-1508826"].stop_times[1],
    (WorldTripStopTimes{
      .stop_id = "bart-place_SFIA",
      .arrival_time = WorldTime(6 * 3600 + 17 * 60),
      .departure_time = WorldTime(6 * 3600 + 19 * 60)
    })
  );
  EXPECT_EQ(
    world.trips["bart-1508826"].stop_times[2],
    (WorldTripStopTimes{
      .stop_id = "bart-place_SBRN",
      .arrival_time = WorldTime(6 * 3600 + 23 * 60),
      .departure_time = WorldTime(6 * 3600 + 23 * 60)
    })
  );

  EXPECT_EQ(
    world.trips["bart-1508916"].stop_times[21],
    (WorldTripStopTimes{
      .stop_id = "bart-place_SBRN",
      .arrival_time = WorldTime(6 * 3600 + 38 * 60),
      .departure_time = WorldTime(6 * 3600 + 39 * 60)
    })
  );
  EXPECT_EQ(
    world.trips["bart-1508916"].stop_times[22],
    (WorldTripStopTimes{
      .stop_id = "bart-place_SFIA",
      .arrival_time = WorldTime(6 * 3600 + 43 * 60),
      .departure_time = WorldTime(6 * 3600 + 46 * 60)
    })
  );
  EXPECT_EQ(
    world.trips["bart-1508916"].stop_times[23],
    (WorldTripStopTimes{
      .stop_id = "bart-place_MLBR",
      .arrival_time = WorldTime(6 * 3600 + 50 * 60),
      .departure_time = WorldTime(6 * 3600 + 50 * 60)
    })
  );
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
