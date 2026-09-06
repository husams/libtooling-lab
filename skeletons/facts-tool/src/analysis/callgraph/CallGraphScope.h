#pragma once

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphRequest.h"

#include <expected>

namespace facts::callgraph {

std::expected<ScopeSelection, std::string>
resolveScope(std::vector<std::string> components, CallsScope calls,
             const CoverageReport &coverage);

std::string exclusionReason(const QueryNode &node, const ScopeSelection &scope,
                            const CoverageReport *coverage);

inline bool included(const QueryNode &node, const ScopeSelection &scope,
                     const CoverageReport *coverage) {
  return exclusionReason(node, scope, coverage).empty();
}

} // namespace facts::callgraph
