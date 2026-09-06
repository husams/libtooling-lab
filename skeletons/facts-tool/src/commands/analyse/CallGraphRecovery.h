#pragma once

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphQuery.h"
#include "analysis/callgraph/CallGraphRecoveryTypes.h"

#include <span>
#include "cli/Options.h"

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace facts::commands {

using RecoveryEntry = callgraph::RecoveryEntry;
using RecoveryReport = callgraph::RecoveryReport;

struct RecoveryResult {
  callgraph::QueryGraph graph;
  std::optional<callgraph::CoverageReport> coverage;
  RecoveryReport report;
};

std::expected<RecoveryResult, std::string>
recoverCallGraph(const cli::CallGraphOptions &options,
                 callgraph::QueryGraph graph,
                 std::optional<callgraph::CoverageReport> coverage,
                 std::span<const SymbolId> roots);

} // namespace facts::commands
