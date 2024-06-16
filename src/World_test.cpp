#include <algorithm>
#include <iterator>

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

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
