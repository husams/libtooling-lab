#pragma once

#include "cli/Options.h"
#include "commands/CompilationDatabase.h"
#include "commands/match/MatchOutput.h"
#include "commands/match/BindingPolicy.h"

#include <expected>
#include <string>
#include <vector>

namespace facts {
class FileManager;
}

namespace facts::commands::match {

std::expected<MatchOutput, std::string>
execute(const cli::MatchOptions &options, CompilationDatabasePtr database,
        FileManager &files, const std::vector<std::string> &sources,
        const std::string &fingerprint, bool installSignals = true,
        BindingPolicy policy = BindingPolicy::Contract);

} // namespace facts::commands::match
