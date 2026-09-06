#pragma once

#include "analysis/callgraph/CallGraphQuery.h"

#include <string_view>

namespace facts::callgraph {

std::expected<const QueryNode *, std::string>
selectOne(const QueryGraph &graph, std::string_view selector,
          std::string_view role);

} // namespace facts::callgraph
