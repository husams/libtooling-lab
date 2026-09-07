#pragma once

#include "cli/Options.h"

#include <expected>
#include <string>

namespace facts::commands {
std::expected<int, std::string>
runCallGraphEntry(const cli::CallGraphEntryOptions &options);
}
