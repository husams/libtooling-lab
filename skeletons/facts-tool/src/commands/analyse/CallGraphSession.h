#pragma once
#include "analysis/callgraph/CallGraphCoverage.h"
#include "cli/Options.h"

namespace facts::commands {
std::expected<cli::CallGraphOptions, std::string>
resolveGraphSession(cli::CallGraphOptions options);
std::expected<void, std::string>
validateGraphOutput(const cli::CallGraphOptions &options,
                    const callgraph::CoverageReport *coverage);
} // namespace facts::commands
