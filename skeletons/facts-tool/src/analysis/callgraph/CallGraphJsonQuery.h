#pragma once

#include "analysis/callgraph/CallGraphJsonDetail.h"
#include "analysis/callgraph/CallGraphSearch.h"

namespace facts::callgraph::detail {

void addQueryJson(llvm::json::Object &output, QueryMode mode,
                  std::optional<PathMode> pathMode, const QueryNode *target,
                  const std::vector<QueryPath> &paths, std::string_view result);

} // namespace facts::callgraph::detail
