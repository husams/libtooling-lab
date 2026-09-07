#include "analysis/callgraph/CallGraphTraversal.h"

#include "analysis/callgraph/CallGraphText.h"
#include "analysis/callgraph/CallGraphTraversalEngine.h"

namespace facts::callgraph {
RenderedGraph renderCallGraph(const QueryGraph &graph,
                              const std::vector<const QueryNode *> &roots,
                              TraversalRequest request,
                              const CoverageReport *coverage, EdgeView view) {
  auto traversal =
      TraversalEngine{graph, std::move(request), coverage}.run(roots);
  traversal.text = renderCallGraphText(graph, roots, traversal, coverage, view);
  return traversal;
}

RenderedGraph renderCallGraph(const QueryGraph &graph,
                              const std::vector<const QueryNode *> &roots,
                              std::optional<int> maxDepth,
                              const CoverageReport *coverage, EdgeView view) {
  TraversalRequest request;
  request.limits.depth = maxDepth;
  return renderCallGraph(graph, roots, std::move(request), coverage, view);
}

} // namespace facts::callgraph
