#include "analysis/callgraph/CallGraphSearch.h"

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
  return "callees";
}

std::string_view pathModeName(PathMode mode) {
  return mode == PathMode::Shortest ? "shortest" : "all-simple";
}

TraversalResult searchCallers(const QueryGraph &graph,
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
