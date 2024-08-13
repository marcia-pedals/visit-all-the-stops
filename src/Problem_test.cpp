#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

#include "Problem.h"

namespace {

// Returns an arbitrary schedule that satisfies the ordering invariant but that is not necessarily
// minimized.
Schedule ArbitraryScheduleNotMinimized() {
  Schedule result;
  if (*rc::gen::arbitrary<bool>()) {
    result.anytime_duration = WorldDuration(*rc::gen::inRange<unsigned int>(0, 1000));
  }
  const size_t num_segments = *rc::gen::inRange<size_t>(0, 20);
  for (size_t i = 0; i < num_segments; ++i) {
    const unsigned int departure_time = *rc::gen::inRange<unsigned int>(0, 1000);
    const unsigned int duration = *rc::gen::inRange<unsigned int>(0, 1000);
    result.segments.push_back(Segment{
      .departure_time = WorldTime(departure_time),
      .arrival_time = WorldTime(departure_time + duration)
    });
  }
  std::sort(result.segments.begin(), result.segments.end(), SegmentComp);
  return result;
}

bool IsMinimalSchedule(const Schedule& schedule) {
  if (schedule.anytime_duration.has_value()) {
    for (const Segment& seg : schedule.segments) {
      if (seg.arrival_time.seconds - seg.departure_time.seconds >= schedule.anytime_duration->seconds) {
        return false;
      }
    }
  }
  for (size_t i = 0; i < schedule.segments.size(); ++i) {
    for (size_t j = i + 1; j < schedule.segments.size(); ++j) {
      const Segment& seg1 = schedule.segments[i];
      const Segment& seg2 = schedule.segments[j];
      if (
        (seg1.departure_time.seconds == seg2.departure_time.seconds) ||
        (seg1.departure_time.seconds > seg2.departure_time.seconds && seg1.arrival_time.seconds <= seg2.arrival_time.seconds) ||
        (seg1.departure_time.seconds < seg2.departure_time.seconds && seg1.arrival_time.seconds >= seg2.arrival_time.seconds)
      ) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

RC_GTEST_PROP(
  ProblemTest,
  eraseNonMinimalProducesMinimal,
  ()
) {
  Schedule schedule = ArbitraryScheduleNotMinimized();
  EraseNonMinimal(schedule);
  RC_ASSERT(IsMinimalSchedule(schedule));
}


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
