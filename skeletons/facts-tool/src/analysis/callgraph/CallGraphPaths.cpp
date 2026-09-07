#include "analysis/callgraph/CallGraphPathSearch.h"

#include "analysis/callgraph/CallGraphOrder.h"

#include <ranges>

namespace facts::callgraph::detail {

PathSearch::PathSearch(const QueryGraph &graph, const QueryNode &target,
                       TraversalRequest request, const CoverageReport *coverage)
    : graph_(graph), target_(target), maxDepth_(request.limits.depth),
      coverage_(coverage),
      budget_(std::move(request), coverage, result_.traversal) {}

QuerySearch PathSearch::run(const QueryNode &source, PathMode mode) {
  PathState initial{{{source.id}, {}}, {source.id, {}, {}}};
  if (!budget_.root(source))
    return std::move(result_);
  if (source.id == target_.id)
    result_.paths.push_back(initial.path);
  else if (mode == PathMode::Shortest)
    shortest(std::move(initial));
  else
    allSimple(std::move(initial));
  if (!result_.paths.empty())
    retainPaths();
  return std::move(result_);
}

bool PathSearch::capped(const PathState &state) const {
  return maxDepth_ &&
         state.path.edges.size() >= static_cast<std::size_t>(*maxDepth_);
}

bool PathSearch::visit(const PathState &state, const QueryEdge &edge,
                       const QueryNode &node, bool reused) {
  if (!budget_.admit(node, &edge))
    return false;
  recordEdge(result_.traversal.edges, edge, state.path.edges.size() + 1, false,
             reused, false, boundaries(node, coverage_));
  return true;
}

std::vector<const QueryEdge *> PathSearch::eligible(const PathState &state) {
  auto edges = orderedEdges(graph_, state.context.node);
  std::erase_if(edges, [&](const auto *edge) {
    const auto *node = findSearchNode(graph_, edge->destination);
    return !matchesContext(*edge, state.context) ||
           std::ranges::contains(state.path.nodes, edge->destination) ||
           !node || !budget_.include(*node, edge);
  });
  return edges;
}

PathState PathSearch::descend(const PathState &state,
                              const QueryEdge &edge) const {
  auto next = state;
  next.path.nodes.push_back(edge.destination);
  next.path.edges.push_back(edge);
  next.context = descendContext(edge, state.context);
  return next;
}

void PathSearch::retainPaths() {
  result_.traversal.nodes.clear();
  result_.traversal.edges.clear();
  for (const auto &path : result_.paths) {
    for (const auto id : path.nodes)
      recordNode(result_.traversal.nodes, id);
    for (std::size_t index = 0; index < path.edges.size(); ++index) {
      const auto *node = findSearchNode(graph_, path.edges[index].destination);
      recordEdge(result_.traversal.edges, path.edges[index], index + 1, false,
                 false, false,
                 node ? boundaries(*node, coverage_) : std::pair{false, false});
    }
  }
}

} // namespace facts::callgraph::detail

namespace facts::callgraph {
QuerySearch searchPathsWithRequest(const QueryGraph &graph,
                                   const QueryNode &source,
                                   const QueryNode &target, PathMode mode,
                                   TraversalRequest request,
                                   const CoverageReport *coverage) {
  return detail::PathSearch{graph, target, std::move(request), coverage}.run(
      source, mode);
}

} // namespace facts::callgraph
