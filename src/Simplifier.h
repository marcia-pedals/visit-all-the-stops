#pragma once

#include "Problem.h"

// Returns a simplified version of the problem that only keeps `keep_stop_ids`.
//
// The segments in this problem are all "minimal" segments between all pairs of `keep_stop_ids`. A
// "minimal" segment is route using any available transit/walking that
// - does not go through any `keep_stop_ids` other than the origin and destination,
// - arrives at the destination at or earlier than any other route leaving the origin at or later.
Problem SimplifyProblem(const Problem &problem, const std::vector<std::string> &keep_stop_ids);
