#include "analysis/callgraph/CallGraphTraversal.h"

#include "analysis/callgraph/CallGraphTraversalEngine.h"

namespace facts::callgraph {
TraversalResult traverseCallGraph(const QueryGraph &graph,
                                  const std::vector<const QueryNode *> &roots,
                                  TraversalRequest request,
                                  const CoverageReport *coverage) {
  return TraversalEngine{graph, std::move(request), coverage}.run(roots);
}

TraversalResult traverseCallGraph(const QueryGraph &graph,
                                  const std::vector<const QueryNode *> &roots,
                                  std::optional<int> maxDepth,
                                  const CoverageReport *coverage) {
  TraversalRequest request;
  request.limits.depth = maxDepth;
  return traverseCallGraph(graph, roots, std::move(request), coverage);
}

} // namespace facts::callgraph
