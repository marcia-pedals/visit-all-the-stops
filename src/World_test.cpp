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
    std::chrono::year_month_day{std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{day}},
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
  auto actual = readServiceIdsVec("data/fetched-2024-06-08/bart", "bart-", 2024, 6, 7);
  std::vector<std::string> expected{
      "bart-2024_01_15-DX-MVS-Weekday-01",
      "bart-2024_01_15-DX19-Weekday-01",
      "bart-2024_01_15-DX20-Weekday-01"
  };
  std::sort(expected.begin(), expected.end());
  EXPECT_EQ(actual, expected);
}

TEST(
  WorldTest,
  readServiceIdsBartSaturday
) {
  auto actual = readServiceIdsVec("data/fetched-2024-06-08/bart", "bart-", 2024, 6, 8);
  std::vector<std::string> expected{
      "bart-2024_01_15-SA-MVS-Saturday-01",
      "bart-2024_01_15-SA19-Saturday-01",
      "bart-2024_01_15-SA20-Saturday-01"
  };
  std::sort(expected.begin(), expected.end());
  EXPECT_EQ(actual, expected);
}

TEST(
  WorldTest,
  readServiceIdsBartSunday
) {
  auto actual = readServiceIdsVec("data/fetched-2024-06-08/bart", "bart-", 2024, 6, 9);
  std::vector<std::string> expected{
      "bart-2024_01_15-SU-MVS-Sunday-01",
      "bart-2024_01_15-SU19-Sunday-01",
      "bart-2024_01_15-SU20-Sunday-01"
  };
  std::sort(expected.begin(), expected.end());
  EXPECT_EQ(actual, expected);
}

TEST(
  WorldTest,
  readServiceIdsBartFourthOfJuly
) {
  auto actual = readServiceIdsVec("data/fetched-2024-06-08/bart", "bart-", 2024, 7, 4);
  std::vector<std::string> expected{
      "bart-2024_01_15-SU-MVS-Sunday-01",
      "bart-2024_01_15-SU19-Sunday-01",
      "bart-2024_01_15-SU20-Sunday-01"
  };
  std::sort(expected.begin(), expected.end());
  EXPECT_EQ(actual, expected);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
