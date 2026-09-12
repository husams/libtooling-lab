#pragma once

#include "commands/CompilationDatabase.h"
#include "commands/IncludedFiles.h"

#include <expected>
#include <span>
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

std::expected<void, std::string>
requireRegisteredFiles(FileManager &files,
                       std::span<const std::string> visitedSources,
                       const std::string &fingerprint);

// Validates that `sources` and everything they transitively include are
// registered, and returns the full per-source discovery result (each
// source's own transitive include set, plus the merged union that was
// validated) so a caller does not have to preprocess a second time to know
// what it just discovered.
std::expected<DiscoveredIncludes, std::string> requireRegisteredSources(
    FileManager &files, const clang::tooling::CompilationDatabase &database,
    const std::vector<std::string> &sources, const std::string &fingerprint);

} // namespace facts::commands
