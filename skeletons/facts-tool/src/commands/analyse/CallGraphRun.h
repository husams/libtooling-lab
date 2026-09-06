#pragma once

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphQuery.h"
#include "commands/analyse/CallGraphRequest.h"
#include "analysis/callgraph/CallGraphRecoveryTypes.h"

namespace facts::commands {

std::expected<int, std::string>
runSelectedCallGraph(const cli::CallGraphOptions &options,
                     const CallGraphRequest &request,
                     const callgraph::QueryGraph &graph,
                     const callgraph::CoverageReport *coverage);

std::expected<int, std::string> runCallGraphQuery(
    const cli::CallGraphOptions &options, const CallGraphRequest &request,
    const callgraph::QueryGraph &graph, const callgraph::CoverageReport *coverage,
    const callgraph::RecoveryReport *recovery);

} // namespace facts::commands
