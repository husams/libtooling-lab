#pragma once

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphQuery.h"
#include "commands/analyse/CallGraphRequest.h"
#include "commands/analyse/CallGraphRunRecord.h"

namespace facts::commands {

// Validates root, target, scope and budget selectors before any traversal so
// usage and configuration errors never write a run.
std::expected<void, std::string>
validateCallGraphSelection(const cli::CallGraphOptions &options,
                           const CallGraphRequest &request,
                           const callgraph::QueryGraph &graph,
                           const callgraph::CoverageReport *coverage);

// Traverses the stored graph once and describes the run to persist.
std::expected<CallGraphRunRecord, std::string>
runStoredCallGraph(const cli::CallGraphOptions &options,
                   const CallGraphRequest &request,
                   const callgraph::QueryGraph &graph,
                   const callgraph::CoverageReport *coverage);

// Recovers missing evidence and describes the final graph generation.
std::expected<CallGraphRunRecord, std::string>
runRecoveredCallGraph(const cli::CallGraphOptions &options,
                      const CallGraphRequest &request,
                      const callgraph::QueryGraph &graph,
                      const callgraph::CoverageReport *coverage);

} // namespace facts::commands
