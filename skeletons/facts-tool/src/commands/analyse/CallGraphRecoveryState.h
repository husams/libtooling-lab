#pragma once
#include "commands/analyse/CallGraphRecoveryInternal.h"

namespace facts::commands {
std::vector<SymbolId> recoveryReachable(const callgraph::QueryGraph &graph,
                                      std::span<const SymbolId> roots);
void preserveRecoveryEvidence(RecoveryContext &context,
                              const RecoveryResult &result,
                              std::span<const SymbolId> roots,
                              bool freshlyPublished = false);
std::expected<std::string, std::string> recoveryInputVersion(
    const RecoveryContext &context, recovery::InputDigestCache &digests);
void recoveryFailure(RecoveryReport &report, std::string reason);
} // namespace facts::commands
