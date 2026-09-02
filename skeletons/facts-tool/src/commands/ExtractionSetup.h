#pragma once

#include "commands/CompilationDatabase.h"

#include <expected>
#include <string>
#include <vector>

namespace facts {
class FileManager;
}

namespace facts::commands {

std::expected<CompilationDatabasePtr, std::string>
requireStoredCommands(CompilationDatabasePtr database);

std::vector<std::string>
selectSources(const clang::tooling::CompilationDatabase &database,
              const std::vector<std::string> &requested);

std::expected<std::string, std::string>
requireCompletedRegistry(FileManager &files);

std::expected<void, std::string> requireRegisteredSources(
    FileManager &files, const clang::tooling::CompilationDatabase &database,
    const std::vector<std::string> &sources, const std::string &fingerprint);

} // namespace facts::commands
