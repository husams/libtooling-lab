#pragma once

#include "cli/Options.h"
#include "config/Configuration.h"

#include <expected>
#include <string>

namespace facts::commands {

std::expected<int, std::string> runImport(const cli::ImportOptions &options);
std::expected<int, std::string> runImportResolved(const cli::ImportOptions &options,
                                                const config::Resolved &configuration);
// Refresh dependency registrations using stored commands, preserving all
// file-level compiler overrides. Used after native catalog updates.
std::expected<void, std::string> refreshImportRegistry(
    const config::Resolved &configuration, const std::vector<std::string> &sources);

} // namespace facts::commands
