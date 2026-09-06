#include "analysis/callgraph/CallGraphTraversal.h"

#include "analysis/callgraph/CallGraphText.h"
#include "analysis/callgraph/CallGraphTraversalEngine.h"

namespace facts::callgraph {
RenderedGraph renderCallGraph(const QueryGraph &graph,
                              const std::vector<const QueryNode *> &roots,
                              TraversalRequest request,
                              const CoverageReport *coverage) {
  auto traversal =
      TraversalEngine{graph, std::move(request), coverage}.run(roots);
  traversal.text = renderCallGraphText(graph, roots, traversal, coverage);
  return traversal;
}

} // namespace facts::callgraph
