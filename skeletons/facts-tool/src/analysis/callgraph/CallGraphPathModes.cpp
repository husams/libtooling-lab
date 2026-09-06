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
    const auto edges = eligible(state);
    if (capped(state)) {
      result_.traversal.truncated += !edges.empty();
      continue;
    }
    for (const auto *edge : edges) {
      const auto *node = findSearchNode(graph_, edge->destination);
      if (!node)
        continue;
      auto next = descend(state, *edge);
      const auto reused = seen.contains(next.context);
      visit(state, *edge, *node, reused);
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
  const auto edges = eligible(state);
  if (capped(state)) {
    result_.traversal.truncated += !edges.empty();
    return;
  }
  for (const auto *edge : edges) {
    const auto *node = findSearchNode(graph_, edge->destination);
    if (!node)
      continue;
    auto next = descend(state, *edge);
    visit(state, *edge, *node);
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
