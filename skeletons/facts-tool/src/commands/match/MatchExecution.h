#pragma once

#include "cli/Options.h"
#include "commands/CompilationDatabase.h"

#include <expected>
#include <string>
#include <vector>

namespace facts {
class FileManager;
}

namespace facts::commands::match {

std::expected<int, std::string>
execute(const cli::MatchOptions &options, CompilationDatabasePtr database,
        FileManager &files, const std::vector<std::string> &sources);

} // namespace facts::commands::match
