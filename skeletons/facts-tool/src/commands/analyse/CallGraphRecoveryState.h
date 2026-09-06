#pragma once
#include "commands/analyse/CallGraphRecoveryInternal.h"

namespace facts::commands {
std::vector<SymbolId>
recoveryReachable(const callgraph::QueryGraph &graph,
                  std::span<const SymbolId> roots,
                  std::optional<unsigned> maxDepth = std::nullopt);
void preserveRecoveryEvidence(RecoveryContext &context,
                              const RecoveryResult &result,
                              std::span<const SymbolId> roots,
                              bool freshlyPublished = false,
                              std::optional<unsigned> maxDepth = std::nullopt);
bool retainRecoveryInputs(RecoveryContext &before, RecoveryContext &after);
void recoveryFailure(RecoveryReport &report, std::string reason);
} // namespace facts::commands
