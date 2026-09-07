#pragma once

#include "model/CallGraphEntry.h"

#include <expected>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace facts::cli {
struct CallGraphOptions;
}

namespace facts {
class FactStore;

namespace callgraph {
struct QueryGraph;
}

namespace commands {
struct RecoveryCandidate;
struct RecoveryContext;

struct RecoveryEvidence {
  std::vector<CallGraphEntry> entries;
};

RecoveryEvidence makeRecoveryEvidence(std::span<const SymbolId> symbols);
std::expected<RecoveryEvidence, std::string>
validateRecoveryEvidence(const RecoveryContext &, const cli::CallGraphOptions &,
                         const callgraph::QueryGraph &,
                         const RecoveryCandidate &);
std::expected<void, std::error_code>
publishRecoveryEvidence(FactStore &store, const RecoveryEvidence &evidence);
} // namespace commands
} // namespace facts
