#pragma once

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphRequest.h"
#include "analysis/callgraph/CallGraphSearch.h"
#include "cli/Options.h"

#include <expected>

namespace facts::commands {

struct CallGraphRequest {
  callgraph::QueryMode mode = callgraph::QueryMode::Callees;
  callgraph::PathMode pathMode = callgraph::PathMode::Shortest;
};

std::expected<CallGraphRequest, std::string>
validateCallGraphRequest(const cli::CallGraphOptions &options);
std::expected<callgraph::TraversalRequest, std::string>
makeCallGraphRequest(const cli::CallGraphOptions &options,
                     const callgraph::CoverageReport *coverage,
                     std::function<bool()> cancelled);

} // namespace facts::commands
