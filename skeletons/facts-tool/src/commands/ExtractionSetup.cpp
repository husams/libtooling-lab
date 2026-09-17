#include "commands/ExtractionSetup.h"

#include "commands/IncludedFiles.h"
#include "platform/PlatformFlags.h"
#include "storage/FileManager.h"

#include <algorithm>
#include <ranges>
#include <set>
#include <string_view>

namespace facts::commands {

std::expected<CompilationDatabasePtr, std::string>
requireStoredCommands(CompilationDatabasePtr database) {
  if (database->getAllCompileCommands().empty())
    return std::unexpected("project configuration is incomplete; run "
                           "'facts-tool import' to rebuild it");
  return database;
}

std::vector<std::string>
selectSources(const clang::tooling::CompilationDatabase &database,
              const std::vector<std::string> &requested) {
  std::vector<std::string> sources;
  std::set<std::string> seen;
  const auto append = [&](const std::filesystem::path &path) {
    auto identity = std::filesystem::absolute(path).lexically_normal().string();
    if (seen.insert(identity).second)
      sources.push_back(std::move(identity));
  };
  const auto appendCommand = [&](const auto &command) {
    const std::filesystem::path source(command.Filename);
    append(source.is_absolute()
               ? source
               : std::filesystem::path(command.Directory) / source);
  };
  if (requested.empty()) {
    for (const auto &command : database.getAllCompileCommands())
      appendCommand(command);
    return sources;
  }
  for (const auto &candidate : requested) {
    const auto commands = database.getCompileCommands(candidate);
    if (commands.empty()) {
      append(candidate);
      continue;
    }
    for (const auto &command : commands)
      appendCommand(command);
  }
  return sources;
}

std::expected<std::string, std::string>
requireCompletedRegistry(FileManager &files) {
  return files.registryStatus()
      .transform_error([](std::error_code error) {
        return "cannot read the file registry state: " + error.message();
      })
      .and_then([](const RegistryStatus &status)
                    -> std::expected<std::string, std::string> {
        if (!status.complete)
          return std::unexpected(
              "project configuration is incomplete; run 'facts-tool import' "
              "to rebuild it: no import has completed the file registry");
        return status.fingerprint;
      });
}

namespace {
std::string toolchainDrift(const std::string &imported) {
  const auto current = platformFingerprint();
  return imported == current
             ? std::string{}
             : " (the toolchain moved since import: imported under " +
                   imported + ", running under " + current + ")";
}
} // namespace

std::expected<void, std::string>
requireRegisteredFiles(FileManager &files,
                       std::span<const std::string> visitedSources,
                       const std::string &fingerprint) {
  const std::set<std::string_view> uniqueSources(visitedSources.begin(),
                                                 visitedSources.end());
  const auto missing = std::ranges::find_if(
      uniqueSources,
      [&](const auto &source) { return !files.getId(source); });
  return missing == uniqueSources.end()
             ? std::expected<void, std::string>{}
             : std::expected<void, std::string>{std::unexpected(
                   "project configuration is incomplete; run 'facts-tool "
                   "import' to rebuild it: " +
                   std::string{*missing} + toolchainDrift(fingerprint))};
}

std::expected<DiscoveredIncludes, std::string> requireRegisteredSources(
    FileManager &files, const clang::tooling::CompilationDatabase &database,
    const std::vector<std::string> &sources, const std::string &fingerprint,
    const astcache::Options &cache) {
  return discoverIncludedFilesPerSource(database, sources, cache)
      .and_then([&](DiscoveredIncludes discovered)
                    -> std::expected<DiscoveredIncludes, std::string> {
        return requireRegisteredFiles(files, discovered.merged, fingerprint)
            .transform([&] { return std::move(discovered); });
      });
}

} // namespace facts::commands
