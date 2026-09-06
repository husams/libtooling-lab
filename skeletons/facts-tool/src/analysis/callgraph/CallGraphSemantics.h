#pragma once

#include "analysis/callgraph/CallGraphQuery.h"

#include <string_view>

namespace facts::callgraph {

enum class EdgeView { Semantic, Calls };

std::string_view edgeViewName(EdgeView view);
std::string_view semanticKind(const QueryNode *target, RelationKind relation);

} // namespace facts::callgraph
