#pragma once

#include "commands/analyse/RecoveryEvidenceScanner.h"

namespace facts::callgraph {
struct QueryGraph;
}

namespace facts::commands {
std::expected<RecoveryPointerEvidence, std::string>
collectRecoveryPointerEvidence(const RecoveryContext &context,
                               const callgraph::QueryGraph &graph);
} // namespace facts::commands
