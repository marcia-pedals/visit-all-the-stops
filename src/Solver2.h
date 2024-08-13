#pragma once

#include "Problem.h"

struct DenseProblem {
  // entries[from * num_stops + to] is the schedule from `from` to `to`.
  std::vector<Schedule> entries;
};

DenseProblem MakeDenseProblem(const Problem& problem);
