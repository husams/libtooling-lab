#include "analysis/callgraph/CallGraphPathSearch.h"

#include "analysis/callgraph/CallGraphOrder.h"

#include <ranges>

namespace facts::callgraph::detail {

PathSearch::PathSearch(const QueryGraph &graph, const QueryNode &target,
                       std::optional<int> maxDepth,
                       const CoverageReport *coverage)
    : graph_(graph), target_(target), maxDepth_(maxDepth), coverage_(coverage) {
}

QuerySearch PathSearch::run(const QueryNode &source, PathMode mode) {
  PathState initial{{{source.id}, {}}, {source.id, {}, {}}};
  recordNode(result_.traversal.nodes, source.id);
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

void PathSearch::visit(const PathState &state, const QueryEdge &edge,
                       const QueryNode &node, bool reused) {
  recordNode(result_.traversal.nodes, node.id);
  recordEdge(result_.traversal.edges, edge, state.path.edges.size() + 1, false,
             reused, false, boundaries(node, coverage_));
}

std::vector<const QueryEdge *>
PathSearch::eligible(const PathState &state) const {
  auto edges = orderedEdges(graph_, state.context.node);
  std::erase_if(edges, [&](const auto *edge) {
    return !matchesContext(*edge, state.context) ||
           std::ranges::contains(state.path.nodes, edge->destination);
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
  const auto truncated = result_.traversal.truncated;
  result_.traversal = {};
  result_.traversal.truncated = truncated;
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
QuerySearch searchPaths(const QueryGraph &graph, const QueryNode &source,
                        const QueryNode &target, PathMode mode,
                        std::optional<int> maxDepth,
                        const CoverageReport *coverage) {
  return detail::PathSearch{graph, target, maxDepth, coverage}.run(source,
                                                                   mode);
}
} // namespace facts::callgraph
