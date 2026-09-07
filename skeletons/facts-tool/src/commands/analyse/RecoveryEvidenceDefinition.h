#pragma once
#include "analysis/callgraph/CallGraphQuery.h"
#include "commands/analyse/RecoveryEvidenceScanner.h"

namespace facts::commands {
std::expected<void, std::string>
validateRecoveryDefinition(const RecoveryContext &,
                           const callgraph::QueryNode &,
                           const RecoveryBodyFacts &);
} // namespace facts::commands
