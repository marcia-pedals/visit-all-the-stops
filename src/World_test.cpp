#include <algorithm>
#include <iterator>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

#include "World.h"

static std::vector<std::string> readServiceIdsVec(
  const std::string& path,
  const std::string& prefix,
  int year,
  unsigned int month,
  unsigned int day
) {
  std::unordered_set<std::string> service_ids;
  readServiceIds(
    path,
    prefix,
    absl::CivilDay(year, month, day),
    service_ids
  );
  std::vector<std::string> result(service_ids.begin(), service_ids.end());
  std::sort(result.begin(), result.end());
  return result;
}

TEST(
  WorldTest,
  readServiceIdsBartWeekday
) {
  EXPECT_THAT(
     readServiceIdsVec("data/fetched-2024-06-08/bart", "bart-", 2024, 6, 7),
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
  EXPECT_THAT(
    readServiceIdsVec("data/fetched-2024-06-08/bart", "bart-", 2024, 6, 8),
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
  EXPECT_THAT(
    readServiceIdsVec("data/fetched-2024-06-08/bart", "bart-", 2024, 6, 9),
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
  EXPECT_THAT(
    readServiceIdsVec("data/fetched-2024-06-08/bart", "bart-", 2024, 7, 4),
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
  EXPECT_THAT(
    readServiceIdsVec("data/fetched-2024-06-08/caltrain", "caltrain-", 2024, 6, 13),
    testing::UnorderedElementsAre("caltrain-72982")
  );
}

TEST(
  WorldTest,
  readServiceIdsCaltrainWeekend
) {
  EXPECT_THAT(
    readServiceIdsVec("data/fetched-2024-06-08/caltrain", "caltrain-", 2024, 6, 16),
    testing::UnorderedElementsAre("caltrain-72981")
  );
}

TEST(
  WorldTest,
  readServiceIdsCaltrainMemorialDay
) {
  EXPECT_THAT(
    readServiceIdsVec("data/fetched-2024-06-08/caltrain", "caltrain-", 2024, 5, 27),
    testing::UnorderedElementsAre("caltrain-72981")
  );
}

TEST(
  WorldTest,
  readServiceIdsCaltrainSpecialElectricTestingShutdown
) {
  EXPECT_THAT(
    readServiceIdsVec("data/fetched-2024-06-08/caltrain", "caltrain-", 2024, 6, 9),
    testing::UnorderedElementsAre()
  );
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
