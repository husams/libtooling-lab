#pragma once

#include "analysis/callgraph/CallGraphTraversal.h"

#include <string_view>

namespace facts::callgraph {

enum class QueryMode { Callees, Callers, Path };
enum class PathMode { Shortest, AllSimple };

struct QueryPath {
  std::vector<SymbolId> nodes;
  std::vector<QueryEdge> edges;
};

struct QuerySearch {
  TraversalResult traversal;
  std::vector<QueryPath> paths;
};

std::string_view queryModeName(QueryMode mode);
std::string_view pathModeName(PathMode mode);
TraversalResult searchCallers(const QueryGraph &graph,
                              const std::vector<const QueryNode *> &roots,
                              std::optional<int> maxDepth,
                              const CoverageReport *coverage = nullptr);
QuerySearch searchPaths(const QueryGraph &graph, const QueryNode &source,
                        const QueryNode &target, PathMode mode,
                        std::optional<int> maxDepth,
                        const CoverageReport *coverage = nullptr);
TraversalResult searchCallersWithRequest(
    const QueryGraph &graph, const std::vector<const QueryNode *> &roots,
    TraversalRequest request, const CoverageReport *coverage = nullptr);
QuerySearch searchPathsWithRequest(const QueryGraph &graph,
                                   const QueryNode &source,
                                   const QueryNode &target, PathMode mode,
                                   TraversalRequest request,
                                   const CoverageReport *coverage = nullptr);

} // namespace facts::callgraph
