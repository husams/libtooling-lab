#pragma once

#include "cli/Options.h"

#include <expected>
#include <string>

namespace facts::commands {

std::expected<int, std::string> runExtract(const cli::ExtractOptions &options);

} // namespace facts::commands
