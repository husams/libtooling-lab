#pragma once

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphRequest.h"
#include "cli/Options.h"

#include <expected>

namespace facts::commands {

std::expected<callgraph::TraversalRequest, std::string>
makeCallGraphRequest(const cli::CallGraphOptions &options,
                     const callgraph::CoverageReport *coverage,
                     std::function<bool()> cancelled);

} // namespace facts::commands
