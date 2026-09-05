#include "commands/ExtractionSetup.h"

#include "commands/IncludedFiles.h"
#include "platform/PlatformFlags.h"
#include "storage/FileManager.h"

#include <algorithm>

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
  return requested.empty() ? database.getAllFiles() : requested;
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

std::expected<void, std::string> requireRegisteredSources(
    FileManager &files, const clang::tooling::CompilationDatabase &database,
    const std::vector<std::string> &sources, const std::string &fingerprint) {
  return discoverIncludedFiles(database, sources)
      .and_then([&](const std::vector<std::string> &included) {
        const auto missing = std::ranges::find_if(
            included, [&](const auto &source) { return !files.getId(source); });
        return missing == included.end()
                   ? std::expected<void, std::string>{}
                   : std::expected<void, std::string>{std::unexpected(
                         "project configuration is incomplete; run 'facts-tool "
                         "import' to rebuild it: " +
                         *missing + toolchainDrift(fingerprint))};
      });
}

} // namespace facts::commands
