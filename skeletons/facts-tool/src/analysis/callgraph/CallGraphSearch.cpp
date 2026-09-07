#include "analysis/callgraph/CallGraphSearch.h"

#include "analysis/callgraph/CallGraphCoverage.h"

#include <ranges>

namespace facts::callgraph {

std::string_view queryModeName(QueryMode mode) {
  switch (mode) {
  case QueryMode::Callers:
    return "callers";
  case QueryMode::Path:
    return "path";
  case QueryMode::Callees:
    return "callees";
  }
}

std::string_view pathModeName(PathMode mode) {
  return mode == PathMode::Shortest ? "shortest" : "all-simple";
}

std::string pathResult(const QueryGraph &graph, const QuerySearch &search,
                       const CoverageReport *coverage) {
  if (search.traversal.truncated)
    return "truncated";
  if (!search.paths.empty())
    return "found";
  if (!coverage)
    return "unknown";
  const auto boundary =
      std::ranges::any_of(search.traversal.edges, [](const auto &edge) {
        return edge.definitionBoundary;
      });
  const auto state =
      summarizeCoverage(*coverage, graph, search.traversal.nodes);
  return !boundary && state == "complete" ? "not_found" : "unknown";
}

RenderedGraph searchCallers(const QueryGraph &graph,
                            const std::vector<const QueryNode *> &roots,
                            std::optional<int> maxDepth,
                            const CoverageReport *coverage) {
  TraversalRequest request;
  request.limits.depth = maxDepth;
  return searchCallersWithRequest(graph, roots, std::move(request), coverage);
}

QuerySearch searchPaths(const QueryGraph &graph, const QueryNode &source,
                        const QueryNode &target, PathMode mode,
                        std::optional<int> maxDepth,
                        const CoverageReport *coverage) {
  TraversalRequest request;
  request.limits.depth = maxDepth;
  return searchPathsWithRequest(graph, source, target, mode, std::move(request),
                                coverage);
}
} // namespace facts::callgraph
