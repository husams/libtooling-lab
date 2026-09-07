#pragma once

#include "analysis/callgraph/CallGraphQuery.h"
#include "analysis/callgraph/CallGraphRequest.h"

#include <optional>
#include <string>
#include <vector>

namespace facts::callgraph {

struct CoverageReport;

struct TraversedEdge {
  QueryEdge edge;
  int depth = 0;
  bool cycle = false;
  bool reused = false;
  bool externalBoundary = false;
  bool definitionBoundary = false;
  bool depthTruncated = false;
};

// The edges actually reached from the selected roots under the selected scope
// and budgets, plus the frontier where traversal stopped.
struct TraversalResult {
  unsigned truncated = 0;
  std::string reason;
  std::vector<SymbolId> nodes;
  std::vector<TraversedEdge> edges;
  std::vector<FrontierNode> frontier;
  std::vector<FrontierNode> excludedNodes;
  std::vector<ExcludedBoundary> excluded;
  ScopeSelection scope;
  TraversalLimits limits;
};

TraversalResult traverseCallGraph(const QueryGraph &graph,
                                  const std::vector<const QueryNode *> &roots,
                                  TraversalRequest request,
                                  const CoverageReport *coverage = nullptr);

TraversalResult traverseCallGraph(const QueryGraph &graph,
                                  const std::vector<const QueryNode *> &roots,
                                  std::optional<int> maxDepth,
                                  const CoverageReport *coverage = nullptr);

} // namespace facts::callgraph
