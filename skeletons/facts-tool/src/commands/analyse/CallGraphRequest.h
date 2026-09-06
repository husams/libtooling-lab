#pragma once

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

} // namespace facts::commands
