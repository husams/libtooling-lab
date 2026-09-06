#pragma once

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphQuery.h"
#include "commands/analyse/CallGraphRequest.h"

namespace facts::commands {

std::expected<int, std::string>
runSelectedCallGraph(const cli::CallGraphOptions &options,
                     const CallGraphRequest &request,
                     const callgraph::QueryGraph &graph,
                     const callgraph::CoverageReport *coverage);

} // namespace facts::commands
