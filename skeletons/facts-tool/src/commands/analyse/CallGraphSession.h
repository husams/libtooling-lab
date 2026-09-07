#pragma once
#include "cli/Options.h"

#include <expected>
#include <string>

namespace facts::commands {
// Resolves the project/facts pair from explicit options or configuration and
// canonicalises the facts path before anything is opened.
std::expected<cli::CallGraphOptions, std::string>
resolveGraphSession(cli::CallGraphOptions options);
} // namespace facts::commands
