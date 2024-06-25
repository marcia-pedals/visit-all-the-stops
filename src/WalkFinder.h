#pragma once

#include <vector>

struct AdjacencyList {
  // edges[i] are the indices of the stops that can be reached from stop i.
  std::vector<std::vector<size_t>> edges;
};

template <typename WalkVisitor, size_t MaxStops>
void FindAllMinimalWalksDFS(WalkVisitor& visitor, const AdjacencyList& adjacency_list, size_t start);

struct CollectorWalkVisitor {
  std::vector<std::vector<size_t>> walks;
  std::vector<size_t> current;

  void PushStop(size_t index) {
    current.push_back(index);
  }

  void PopStop() {
    current.pop_back();
  }

  void WalkDone() {
    walks.push_back(current);
  }
};
