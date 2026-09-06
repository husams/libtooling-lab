#pragma once

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphRecoveryTypes.h"
#include "analysis/callgraph/CallGraphSearch.h"
#include "analysis/callgraph/CallGraphSemantics.h"
#include "analysis/callgraph/CallGraphTraversal.h"

namespace facts::callgraph {

std::string renderCallGraphJson(
    const QueryGraph &graph, const std::vector<const QueryNode *> &roots,
    const RenderedGraph &traversal, const CoverageReport *coverage,
    EdgeView view = EdgeView::Semantic, QueryMode mode = QueryMode::Callees,
    std::optional<PathMode> pathMode = std::nullopt,
    const QueryNode *target = nullptr, const std::vector<QueryPath> &paths = {},
    std::string_view result = {}, const RecoveryReport *recovery = nullptr);

} // namespace facts::callgraph
