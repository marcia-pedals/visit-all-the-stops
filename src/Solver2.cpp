#include "Solver2.h"

#include <queue>

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
  DenseProblem result;
  result.num_stops = problem.edges.size();
  result.entries = std::vector<Schedule>(result.num_stops * result.num_stops);
  for (size_t from = 0; from < result.num_stops; ++from) {
    for (const Edge& edge: problem.edges[from]) {
      result.entries[from * result.num_stops + edge.destination_stop_index] = edge.schedule;
    }
  }

  for (size_t intermediate = 0; intermediate < result.num_stops; ++intermediate) {
    if (problem.stop_index_to_id[intermediate] == "DUMMY") {
      continue;
    }
    for (size_t from = 0; from < result.num_stops; ++from) {
      for (size_t to = 0; to < result.num_stops; ++to) {
        if (intermediate == from || intermediate == to || from == to) {
          continue;
        }
        MergeIntoSchedule(
          GetMinimalConnectingSchedule(
            result.entries[from * result.num_stops + intermediate],
            result.entries[intermediate * result.num_stops + to],
            /*min_transfer_seconds=*/ 0
          ),
          result.entries[from * result.num_stops + to]
        );
      }
    }
  }

  return result;
}

size_t find_set_index(size_t i, const std::vector<bool>& x) {
  while (i < x.size() && !x[i]) {
    ++i;
  }
  return i;
}

size_t CostMatrix::next_from(size_t i) const {
  return find_set_index(i, from_active);
}

size_t CostMatrix::next_to(size_t i) const {
  return find_set_index(i, to_active);
}

void CostMatrix::CommitEdge(size_t from, size_t to) {
    const size_t num_stops = to_active.size();

    // std::cout << "committing " << from << " -> " << to << "\n";
    const size_t ultimate_to = linked_to[to];
    const size_t ultimate_from = linked_from[from];
    // std::cout << "  ultimate from " << ultimate_from << "\n";
    // std::cout << "  ultimate to " << ultimate_to << "\n";

    // Keep linked_* up to date.
    linked_to[linked_from[from]] = ultimate_to;
    linked_from[linked_to[to]] = ultimate_from;

    // Forbid the loop back from `ultimate_to` to `ultimate_from`.
    c[ultimate_to * num_stops + ultimate_from] = std::numeric_limits<unsigned int>::max();

    // Cross out "used" .
    from_active[from] = false;
    to_active[to] = false;

    num_committed_edges += 1;
}

CostMatrix MakeInitialCostMatrixFromCosts(size_t num_stops, const std::vector<unsigned int>& c) {
  assert(num_stops * num_stops == c.size());
  CostMatrix initial_cost;
  initial_cost.c = c;
  initial_cost.from_active = std::vector<bool>(num_stops, true);
  initial_cost.to_active = std::vector<bool>(num_stops, true);
  initial_cost.num_committed_edges = 0;

  initial_cost.linked_to = std::vector<size_t>(num_stops);
  initial_cost.linked_from = std::vector<size_t>(num_stops);
  for (size_t i = 0; i < num_stops; ++i) {
    initial_cost.linked_to[i] = i;
    initial_cost.linked_from[i] = i;
  }

  return initial_cost;
}

CostMatrix MakeInitialCostMatrix(const DenseProblem& problem) {
  std::vector<unsigned int> c;
  c.reserve(problem.entries.size());
  for (size_t i = 0; i < problem.entries.size(); ++i) {
    c.push_back(problem.entries[i].lower_bound());
  }
  return MakeInitialCostMatrixFromCosts(problem.num_stops, c);
}

unsigned int ReduceCostMatrix(CostMatrix& cost) {
  unsigned int reduction = 0;
  const size_t num_stops = cost.from_active.size();

  std::vector<unsigned int> to_min_cost(num_stops, std::numeric_limits<unsigned int>::max());

  // Reduce froms.
  for (size_t from = cost.next_from(0); from < num_stops; from = cost.next_from(from + 1)) {
    unsigned int min_cost = std::numeric_limits<unsigned int>::max();
    for (size_t to = cost.next_to(0); to < num_stops; to = cost.next_to(to + 1)) {
      if (cost.c[from * num_stops + to] < min_cost) {
        min_cost = cost.c[from * num_stops + to];
      }
    }
    assert(min_cost < std::numeric_limits<unsigned int>::max());
    reduction += min_cost;
    for (size_t to = cost.next_to(0); to < num_stops; to = cost.next_to(to + 1)) {
      if (cost.c[from * num_stops + to] < std::numeric_limits<unsigned int>::max()) {
        cost.c[from * num_stops + to] -= min_cost;
      }
      if (cost.c[from * num_stops + to] < to_min_cost[to]) {
        to_min_cost[to] = cost.c[from * num_stops + to];
      }
    }
  }

  // Reduce tos.
  for (size_t to = cost.next_to(0); to < num_stops; to = cost.next_to(to + 1)) {
    unsigned int min_cost = to_min_cost[to];
    assert(min_cost < std::numeric_limits<unsigned int>::max());
    reduction += min_cost;
    for (size_t from = cost.next_from(0); from < num_stops; from = cost.next_from(from + 1)) {
      if (cost.c[from * num_stops + to] < std::numeric_limits<unsigned int>::max()) {
        cost.c[from * num_stops + to] -= min_cost;
      }
    }
  }

  return reduction;
}

struct SearchEdge {
  size_t parent;

   // If this is true, this edge excludes from->to.
  // If this is false, this edge includes from->to.
  bool exclude;

  size_t from;
  size_t to;
};

struct SearchNode {
  std::optional<SearchEdge> edge;
  unsigned int lb;
  bool visited = false;
};

struct SearchHeapEntry {
  unsigned int lb;
  size_t node;
};

struct SearchHeapEntryCompare {
 bool operator()(const SearchHeapEntry& l, const SearchHeapEntry& r) {
  return l.lb > r.lb;
 }
};

struct SearchStats {
  size_t right_last_1 = 0;
  size_t right_last_10 = 0;
  size_t right_last_100 = 0;
  size_t right_last_1000 = 0;
  size_t right_last_10000 = 0;
  size_t left_last_1 = 0;
  size_t left_last_10 = 0;
  size_t left_last_100 = 0;
  size_t left_last_1000 = 0;
  size_t left_last_10000 = 0;
  size_t rejected_left = 0;
  size_t rejected_right = 0;
};


unsigned int LittleTSP(const CostMatrix& initial_cost) {
  SearchStats stats;

  const size_t num_stops = initial_cost.from_active.size();

  CostMatrix initial_reduced = initial_cost;
  unsigned int initial_reduction = ReduceCostMatrix(initial_reduced);

  // size_t shay = debug.stop_id_to_index.at("place_SHAY");
  // for (size_t i = 0; i < num_stops; ++i) {
  //   auto stop_id = debug.stop_index_to_id[i];
  //   std::cout << "place_SHAY -> " << stop_id << " = " << initial_cost.c[shay * num_stops + i] << "\n";
  //   std::cout << stop_id << " -> " << "place_SHAY = " << initial_cost.c[i * num_stops + shay] << "\n";
  // }

  std::vector<SearchNode> nodes;
  std::priority_queue<SearchHeapEntry, std::vector<SearchHeapEntry>, SearchHeapEntryCompare> q;
  nodes.push_back(SearchNode{
    .edge = std::nullopt,
    .lb = initial_reduction
  });
  q.push(SearchHeapEntry{
    .lb = nodes.back().lb,
    .node = nodes.size() - 1
  });

  CostMatrix cost = initial_reduced;
  size_t cost_for_node = 0;

  bool right_mode = true;

  unsigned int ub = std::numeric_limits<unsigned int>::max();

  unsigned int num_steps = 0;
  while (!q.empty() || right_mode) {
    size_t top_node_index;
    if (right_mode) {
      top_node_index = nodes.size() - 1;
      while (
        top_node_index > 0 && (
          nodes[top_node_index].visited ||
          (nodes[top_node_index].edge.has_value() && nodes[top_node_index].edge->exclude)
        )
      ) {
        top_node_index -= 1;
      }
      if (top_node_index == 0 && nodes[top_node_index].visited) {
        right_mode = false;
        continue;
      }
    } else {
      SearchHeapEntry top = q.top();
      q.pop();
      top_node_index = top.node;
    }
    if (nodes[top_node_index].visited) {
      continue;
    }
    nodes[top_node_index].visited = true;
    SearchNode top_node = nodes[top_node_index];

    size_t rel_pos = nodes.size() - top_node_index;
    if (top_node.edge.has_value()) {
      if (top_node.edge->exclude) {
        if (rel_pos <= 1) { stats.left_last_1 += 1; }
        if (rel_pos <= 10) { stats.left_last_10 += 1; }
        if (rel_pos <= 100) { stats.left_last_100 += 1; }
        if (rel_pos <= 1000) { stats.left_last_1000 += 1; }
        if (rel_pos <= 10000) { stats.left_last_10000 += 1; }
      } else {
        if (rel_pos <= 1) { stats.right_last_1 += 1; }
        if (rel_pos <= 10) { stats.right_last_10 += 1; }
        if (rel_pos <= 100) { stats.right_last_100 += 1; }
        if (rel_pos <= 1000) { stats.right_last_1000 += 1; }
        if (rel_pos <= 10000) { stats.right_last_10000 += 1; }
      }
    }

    if (top_node.lb >= ub) {
      return ub;
    }

    num_steps += 1;
    if (num_steps % 20000 == 0) {
      // std::cout << "step " << num_steps << ": lb " << top.lb << ", ub " << ub << "\n";
      std::cout << log10(static_cast<double>(num_steps)) << " " << top_node.lb << " " << ub << "\n";
      // std::cout << "  right_last_1     " << stats.right_last_1 << "\n";
      // std::cout << "  right_last_10    " << stats.right_last_10 << "\n";
      // std::cout << "  right_last_100   " << stats.right_last_100 << "\n";
      // std::cout << "  right_last_1000  " << stats.right_last_1000 << "\n";
      // std::cout << "  right_last_10000 " << stats.right_last_10000 << "\n";
      // std::cout << "   left_last_1     " << stats.left_last_1 << "\n";
      // std::cout << "   left_last_10    " << stats.left_last_10 << "\n";
      // std::cout << "   left_last_100   " << stats.left_last_100 << "\n";
      // std::cout << "   left_last_1000  " << stats.left_last_1000 << "\n";
      // std::cout << "   left_last_10000 " << stats.left_last_10000 << "\n";
      std::cout << "  rejected left " << stats.rejected_left << "\n";
      std::cout << "  rejected right " << stats.rejected_right << "\n";
      std::cout << "  total            " << num_steps << "\n";
    }
    // if (num_steps == 500000) {
    //   break;
    // }

    if (num_steps % 10000 == 0) {
      // TODO: maybe need to make sure that right mode actually takes the right branch
      right_mode = true;
    }

    if (top_node_index != cost_for_node) {
      // Rebuild the cost matrix!!
      cost = initial_reduced;
      SearchNode cur = top_node;
      unsigned int reduction = 0;
      while (cur.edge.has_value()) {
        // if (edge->exclude) {
        //   std::cout << "exclude ";
        // }
        // std::cout << edge->from << " -> " << edge->to << "\n";

        if (cur.edge->exclude) {
          cost.c[cur.edge->from * num_stops + cur.edge->to] = std::numeric_limits<unsigned int>::max();
        } else {
          cost.CommitEdge(cur.edge->from, cur.edge->to);
          reduction += cost.c[cur.edge->from * num_stops + cur.edge->to];
        }

        // costcopy.Print();
        // std::cout << "\n";

        cur = nodes[cur.edge->parent];
      }
      reduction += ReduceCostMatrix(cost);
      // costcopy.Print();
      // std::cout << "\n";

      // Super weird subtlety that rebuilding and reducing in this different order could have
      // changed the LB and we need to update the LB with these changes so that further LBs we
      // derive from it are correct.
      //
      // I don't totally understand why this happens and why it is correct to just change the LB.
      top_node.lb = initial_reduction + reduction;

      // if (reduction1 != reduction2) {
      //   std::cout << reduction1 << " " << reduction2 << "\n";
      //   assert(false);
      // }
    }

    if (cost.num_committed_edges == num_stops - 2) {
      if (top_node.lb < ub) {
        ub = top_node.lb;
      }
      if (right_mode) {
        right_mode = false;
      }
      continue;
    }

    // Okay we have the cost matrix at this search node.
    // Time to find the "best" branch.
    std::vector<size_t> from_num_zeros(num_stops);
    std::vector<size_t> to_num_zeros(num_stops);
    std::vector<unsigned int> from_smallest_nonzero(num_stops, std::numeric_limits<unsigned int>::max());
    std::vector<unsigned int> to_smallest_nonzero(num_stops, std::numeric_limits<unsigned int>::max());
    for (size_t from = cost.next_from(0); from < num_stops; from = cost.next_from(from + 1)) {
      for (size_t to = cost.next_to(0); to < num_stops; to = cost.next_to(to + 1)) {
        const unsigned int v = cost.c[from * num_stops + to];
        if (v == 0) {
          from_num_zeros[from] += 1;
          to_num_zeros[to] += 1;
        } else {
          from_smallest_nonzero[from] = std::min(from_smallest_nonzero[from], v);
          to_smallest_nonzero[to] = std::min(to_smallest_nonzero[to], v);
        }
      }
    }

    size_t best_from, best_to;
    unsigned int best_theta = 0;
    for (size_t from = cost.next_from(0); from < num_stops; from = cost.next_from(from + 1)) {
      if (from_num_zeros[from] == 0) {
        continue;
      }
      for (size_t to = cost.next_to(0); to < num_stops; to = cost.next_to(to + 1)) {
        if (cost.c[from * num_stops + to] == 0) {
          unsigned int min_from = from_num_zeros[from] > 1 ? 0 : from_smallest_nonzero[from];
          unsigned int min_to = to_num_zeros[to] > 1 ? 0 : to_smallest_nonzero[to];
          if (
            min_from == std::numeric_limits<unsigned int>::max() ||
            min_to == std::numeric_limits<unsigned int>::max()
          ) {
            best_theta = std::numeric_limits<unsigned int>::max();
            best_from = from;
            best_to = to;
          } else if (min_from + min_to >= best_theta) {
            best_theta = min_from + min_to;
            best_from = from;
            best_to = to;
          }
        }
      }
    }

    // std::cout << "best theta " << best_theta << " " << debug.stop_index_to_id[best_from] << " -> " << debug.stop_index_to_id[best_to] << "\n";

    // Push the "exclude" node.
    if (best_theta < std::numeric_limits<unsigned int>::max() && top_node.lb + best_theta < ub) {
      nodes.push_back(SearchNode{
        .edge = SearchEdge{
          .parent = top_node_index,
          .exclude = true,
          .from = best_from,
          .to = best_to
        },
        .lb = top_node.lb + best_theta
      });
      q.push(SearchHeapEntry{
        .lb = nodes.back().lb,
        .node = nodes.size() - 1
      });
      // std::cout << "theta " << best_theta << "\n";
    } else {
      stats.rejected_left += 1;
    }

    // Push the "include" node.
    cost.CommitEdge(best_from, best_to);
    // for (const unsigned int c : cost.c) {
    //   std::cout << c << " ";
    // }
    // std::cout << "\n";
    const unsigned int branch_reduction = ReduceCostMatrix(cost);
    if (top_node.lb + branch_reduction < ub) {
      nodes.push_back(SearchNode{
        .edge = SearchEdge{
          .parent = top_node_index,
          .exclude = false,
          .from = best_from,
          .to = best_to
        },
        .lb = top_node.lb + branch_reduction
      });
      q.push(SearchHeapEntry{
        .lb = nodes.back().lb,
        .node = nodes.size() - 1
      });
      cost_for_node = nodes.size() - 1;
    } else {
      stats.rejected_right += 1;
    }
    // std::cout << "branch reduction " << branch_reduction << "\n";
  }

  return ub;
}
