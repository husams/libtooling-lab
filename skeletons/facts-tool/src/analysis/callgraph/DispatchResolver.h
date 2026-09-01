#pragma once

#include "analysis/callgraph/CallGraphTypes.h"

#include <span>
#include <vector>

namespace facts::callgraph {

std::vector<CallFact>
resolveDispatchCalls(std::span<const CallFact> calls,
                     std::span<const OverrideFact> overrides);

} // namespace facts::callgraph
