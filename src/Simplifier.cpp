#include "Simplifier.h"

World SimplifyWorld(const World & world, const std::unordered_set<std::string>& keep_stop_ids) {
  // For each pair of keep_stop_ids.
  // For each departure from the first stop.
  // Compute the shortest route to the second stop.
  // If the route does not go through any other keep_stop_ids, add an edge to the simplified world.

  return World();
}
