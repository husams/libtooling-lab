#pragma once

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphQuery.h"
#include "analysis/callgraph/CallGraphRecoveryTypes.h"

#include "cli/Options.h"
#include <functional>
#include <span>

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

using RecoveryObserver =
    std::function<std::expected<std::vector<SymbolId>, std::string>(
        const RecoveryResult &)>;

std::expected<RecoveryResult, std::string> recoverCallGraph(
    const cli::CallGraphOptions &options, callgraph::QueryGraph graph,
    std::optional<callgraph::CoverageReport> coverage,
    std::span<const SymbolId> roots, RecoveryObserver observe = {});

} // namespace facts::commands
