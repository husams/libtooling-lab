#include "analysis/callgraph/CallGraphPathSearch.h"

#include "analysis/callgraph/CallGraphOrder.h"

#include <deque>
#include <set>

namespace facts::callgraph::detail {

void PathSearch::shortest(PathState initial) {
  std::deque<PathState> queue{std::move(initial)};
  std::set<QueryContext> seen{queue.front().context};
  while (!queue.empty() && result_.paths.empty()) {
    auto state = std::move(queue.front());
    queue.pop_front();
    if (budget_.stopped(state.context.node))
      continue;
    const auto edges = eligible(state);
    if (capped(state)) {
      if (!edges.empty())
        budget_.truncate(state.context.node, "max_depth");
      continue;
    }
    for (const auto *edge : edges) {
      const auto *node = findSearchNode(graph_, edge->destination);
      if (!node)
        continue;
      auto next = descend(state, *edge);
      const auto reused = seen.contains(next.context);
      if (budget_.stopped(state.context.node) ||
          !visit(state, *edge, *node, reused))
        break;
      if (node->id == target_.id) {
        result_.paths.push_back(std::move(next.path));
        break;
      }
      const auto boundary = boundaries(*node, coverage_);
      if (!reused && !boundary.first && !boundary.second) {
        seen.insert(next.context);
        queue.push_back(std::move(next));
      }
    }
  }
}

void PathSearch::allSimple(PathState state) {
  if (budget_.stopped(state.context.node))
    return;
  const auto edges = eligible(state);
  if (capped(state)) {
    if (!edges.empty())
      budget_.truncate(state.context.node, "max_depth");
    return;
  }
  for (const auto *edge : edges) {
    const auto *node = findSearchNode(graph_, edge->destination);
    if (!node)
      continue;
    auto next = descend(state, *edge);
    if (budget_.stopped(state.context.node) || !visit(state, *edge, *node))
      return;
    if (node->id == target_.id) {
      result_.paths.push_back(std::move(next.path));
      continue;
    }
    const auto boundary = boundaries(*node, coverage_);
    if (!boundary.first && !boundary.second)
      allSimple(std::move(next));
  }
}

} // namespace facts::callgraph::detail
