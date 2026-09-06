#include "analysis/callgraph/CallGraphTraversal.h"

#include <algorithm>
#include <iostream>

namespace {
using namespace facts;
using namespace facts::callgraph;

bool require(bool value, std::string_view message) {
  if (!value)
    std::cerr << message << '\n';
  return value;
}

QueryGraph rootsOnly() {
  return {{QueryNode{{1, 1}, "a", "usr:a", true},
           QueryNode{{2, 1}, "b", "usr:b", true},
           QueryNode{{3, 1}, "c", "usr:c", true}},
          {}};
}

std::vector<const QueryNode *> roots(const QueryGraph &graph) {
  return {&graph.nodes[0], &graph.nodes[1], &graph.nodes[2]};
}

bool hasFrontier(const RenderedGraph &result, SymbolId id,
                 std::string_view reason) {
  return std::ranges::any_of(result.frontier, [&](const auto &item) {
    return item.id == id && item.reason == reason;
  });
}
} // namespace

int main() {
  const auto graph = rootsOnly();
  TraversalRequest bounded;
  bounded.limits.nodes = 1;
  const auto nodeBound = renderCallGraph(graph, roots(graph), bounded);
  TraversalRequest cancelled;
  cancelled.cancelled = [] { return true; };
  const auto cancelledResult =
      renderCallGraph(graph, roots(graph), std::move(cancelled));
  return require(nodeBound.nodes == std::vector<SymbolId>{{1, 1}},
                 "node budget admitted extra roots") &&
                 require(hasFrontier(nodeBound, {2, 1}, "max_nodes") &&
                             hasFrontier(nodeBound, {3, 1}, "max_nodes"),
                         "node budget omitted skipped roots") &&
                 require(cancelledResult.nodes.empty(),
                         "cancelled traversal admitted a root") &&
                 require(
                     cancelledResult.frontier.size() == 3 &&
                         hasFrontier(cancelledResult, {1, 1}, "cancelled") &&
                         hasFrontier(cancelledResult, {2, 1}, "cancelled") &&
                         hasFrontier(cancelledResult, {3, 1}, "cancelled"),
                     "cancelled traversal omitted skipped roots")
             ? 0
             : 1;
}
