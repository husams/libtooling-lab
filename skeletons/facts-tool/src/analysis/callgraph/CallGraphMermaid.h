#pragma once
#include "analysis/callgraph/CallGraphTraversal.h"

namespace facts::callgraph {
std::string
renderCallGraphMermaid(const QueryGraph &graph, const RenderedGraph &traversal,
                       std::string_view metadata, std::string_view facts,
                       const CoverageReport *coverage, EdgeView view,
                       bool initial, bool recoveryFailed);
} // namespace facts::callgraph
