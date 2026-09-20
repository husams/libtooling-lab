#pragma once

#include "cli/Options.h"

#include <expected>
#include <string>

namespace facts::commands {
struct ExtractionStatistics {
  std::size_t selected = 0, processed = 0, skipped = 0, symbols = 0;
};

std::expected<int, std::string> runExtract(const cli::ExtractOptions &options);
// Shared service entry: configuration, output and compiler settings are resolved.
std::expected<int, std::string>
runExtractResolved(const cli::ExtractOptions &options, bool reportProgress = true,
                   ExtractionStatistics *statistics = nullptr);

} // namespace facts::commands
