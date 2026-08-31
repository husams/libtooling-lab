#pragma once

#include "cli/Options.h"

#include <expected>
#include <string>

namespace facts::commands {

std::expected<int, std::string>
runCallGraph(const cli::CallGraphOptions &options);

} // namespace facts::commands
