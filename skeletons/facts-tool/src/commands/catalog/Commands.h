#pragma once

#include "cli/catalog/Options.h"
#include <expected>
#include <string>

namespace facts::commands {
std::expected<int, std::string>
runRepository(const cli::RepositoryOptions &options);
std::expected<int, std::string>
runComponent(const cli::ComponentOptions &options);
std::expected<int, std::string>
runDirectory(const cli::DirectoryOptions &options);
std::expected<int, std::string> runFile(const cli::FileOptions &options);
std::expected<int, std::string> runSymbol(const cli::SymbolOptions &options);
} // namespace facts::commands
