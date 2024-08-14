#pragma once

#include "Problem.h"

struct DenseProblem {
  size_t num_stops;

  // entries[from * num_stops + to] is the schedule from `from` to `to`.
  std::vector<Schedule> entries;
};

struct CostMatrix {
  // c[from * num_stops + to] is the cost from `from` to `to`.
  std::vector<unsigned int> c;

  std::vector<bool> from_active;
  std::vector<bool> to_active;

  unsigned int num_committed_edges;

  // For stops i without committed incoming edges, linked_to[i] is the farthest stop along the
  // committed outgoing path from stop i. (e.g. linked_to[i] == i if it has no committed outgoing
  // edges). For stops i with committed incoming edges, linked_to[i] is undefined.
  //
  // Similarly for linked_from but in the opposite direction.
  std::vector<size_t> linked_to;
  std::vector<size_t> linked_from;

  size_t next_from(size_t i) const;
  size_t next_to(size_t i) const;

  void CommitEdge(size_t from, size_t to);

  void Print() {
    size_t num_stops = from_active.size();
    for (size_t i = next_from(0); i < num_stops; i = next_from(i + 1)) {
      for (size_t j = next_to(0); j < num_stops; j = next_to(j + 1)) {
        if (c[i * num_stops + j] == std::numeric_limits<unsigned int>::max()) {
          std::cout << "x ";
        } else {
          std::cout << c[i * num_stops + j] << " ";
        }
      }
      std::cout << "\n";
    }
  }
};

DenseProblem MakeDenseProblem(const Problem& problem);
CostMatrix MakeInitialCostMatrixFromCosts(size_t num_stops, const std::vector<unsigned int>& c);
CostMatrix MakeInitialCostMatrix(const DenseProblem& problem);
unsigned int ReduceCostMatrix(CostMatrix& cost);
unsigned int LittleTSP(const CostMatrix& initial_cost);
