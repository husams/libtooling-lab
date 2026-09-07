#pragma once

#include "analysis/callgraph/CallGraphSemantics.h"
#include "analysis/callgraph/CallGraphTraversal.h"

namespace facts::callgraph {

std::string renderCallGraphText(const QueryGraph &graph,
                                const std::vector<const QueryNode *> &roots,
                                const RenderedGraph &traversal,
                                const CoverageReport *coverage,
                                EdgeView view = EdgeView::Semantic);

} // namespace facts::callgraph
