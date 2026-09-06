#pragma once

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphTraversal.h"
#include <llvm/Support/JSON.h>

namespace facts::callgraph::detail {

const QueryNode *findNode(const QueryGraph &graph, SymbolId id);
std::string stableId(SymbolId id);
llvm::json::Object nodeJson(const QueryGraph &graph, const QueryNode &node,
                            const CoverageReport *coverage);
llvm::json::Object edgeJson(const QueryGraph &graph, const TraversedEdge &edge,
                            const CoverageReport *coverage);

} // namespace facts::callgraph::detail
