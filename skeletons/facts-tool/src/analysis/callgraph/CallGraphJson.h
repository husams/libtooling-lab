#pragma once

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphSemantics.h"
#include "analysis/callgraph/CallGraphTraversal.h"

namespace facts::callgraph {

std::string renderCallGraphJson(const QueryGraph &graph,
                                const std::vector<const QueryNode *> &roots,
                                const RenderedGraph &traversal,
                                const CoverageReport *coverage,
                                EdgeView view = EdgeView::Semantic);

} // namespace facts::callgraph
