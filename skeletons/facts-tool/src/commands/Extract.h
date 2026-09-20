#pragma once

#include "cli/Options.h"

#include <expected>
#include <string>

namespace facts::commands {

std::expected<int, std::string> runExtract(const cli::ExtractOptions &options);
// Shared service entry: configuration, output and compiler settings are resolved.
std::expected<int, std::string>
runExtractResolved(const cli::ExtractOptions &options, bool reportProgress = true);

} // namespace facts::commands
