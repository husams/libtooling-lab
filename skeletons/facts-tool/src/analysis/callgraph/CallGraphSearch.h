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
  RenderedGraph traversal;
  std::vector<QueryPath> paths;
};

std::string_view queryModeName(QueryMode mode);
std::string_view pathModeName(PathMode mode);
RenderedGraph searchCallers(const QueryGraph &graph,
                            const std::vector<const QueryNode *> &roots,
                            std::optional<int> maxDepth,
                            const CoverageReport *coverage = nullptr);
QuerySearch searchPaths(const QueryGraph &graph, const QueryNode &source,
                        const QueryNode &target, PathMode mode,
                        std::optional<int> maxDepth,
                        const CoverageReport *coverage = nullptr);
std::string pathResult(const QueryGraph &graph, const QuerySearch &search,
                       const CoverageReport *coverage);

} // namespace facts::callgraph
