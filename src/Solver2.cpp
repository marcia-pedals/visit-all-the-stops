#include "Solver2.h"

#include "Problem.h"

// What we gonna do here?
//
// So first I want to generate the full graph as a dense matrix.
// I need to do some kind of modified Floyd-Warhsall for this.
//
// Then what I do is add the extra "starting" vertex that has 0 cost to each vertex and 0 cost from
// each vertex, so that I'm looking for the optimal tour on this graph.
//
// Then I do a modified Little algorithm on that.
// Well first I can implement a vanilla Little algorithm to see how that feels, and then figure out
// how to modify it.

DenseProblem MakeDenseProblem(const Problem& problem) {
  const size_t num_stops = problem.edges.size();
  DenseProblem result;
  result.entries = std::vector<Schedule>(num_stops * num_stops);
  for (size_t from = 0; from < num_stops; ++from) {
    for (const Edge& edge: problem.edges[from]) {
      result.entries[from * num_stops + edge.destination_stop_index] = edge.schedule;
    }
  }

  for (size_t intermediate = 0; intermediate < num_stops; ++intermediate) {
    for (size_t from = 0; from < num_stops; ++from) {
      for (size_t to = 0; to < num_stops; ++to) {
        if (intermediate == from || intermediate == to) {
          continue;
        }
        MergeIntoSchedule(
          GetMinimalConnectingSchedule(
            result.entries[from * num_stops + intermediate],
            result.entries[intermediate * num_stops + to],
            /*min_transfer_seconds=*/ 0
          ),
          result.entries[from * num_stops + to]
        );
      }
    }
  }

  return result;
}
