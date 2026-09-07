#pragma once

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphRecoveryTypes.h"
#include "analysis/callgraph/CallGraphTraversal.h"

#include <llvm/Support/JSON.h>

#include <span>

namespace facts::callgraph::json {

struct RecoverySections {
  llvm::json::Array candidates;
  llvm::json::Array missingDefinitions;
  llvm::json::Array unresolvedTargets;
  llvm::json::Object extractionCoverage;
};

RecoverySections recoverySections(const QueryGraph &graph,
                                  const CoverageReport *coverage,
                                  std::span<const SymbolId> nodes);
llvm::json::Object recoveryObject(const RecoveryReport &report);
llvm::json::Array recoveryErrors(const RecoveryReport *report);

} // namespace facts::callgraph::json
