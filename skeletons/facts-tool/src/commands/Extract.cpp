#include "commands/Extract.h"

#include "ast/FactExtractor.h"
#include "ast/Indexing.h"
#include "platform/PlatformFlags.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"
#include "tooling/CompilationFiles.h"
#include "tooling/StoredCompilationDatabase.h"

#include <clang/Tooling/Tooling.h>

#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace facts::commands {
namespace {

using CompilationDatabase = clang::tooling::CompilationDatabase;
using CompilationDatabasePtr = std::unique_ptr<CompilationDatabase>;

std::expected<void, std::string>
validateDatabasePaths(const cli::ExtractOptions &options) {
  const auto output =
      std::filesystem::absolute(options.output).lexically_normal();
  const auto configuration =
      std::filesystem::absolute(options.configuration).lexically_normal();
  if (output == configuration) {
    return std::unexpected(
        "output and project configuration require separate databases");
  }
  return {};
}

std::expected<CompilationDatabasePtr, std::string>
requireStoredCommands(CompilationDatabasePtr database) {
  if (database->getAllCompileCommands().empty()) {
    return std::unexpected(
        "project configuration contains no stored compile commands");
  }
  return database;
}

std::vector<std::string>
selectSources(const CompilationDatabase &database,
              const std::vector<std::string> &requested) {
  return requested.empty() ? database.getAllFiles() : requested;
}

std::expected<void, std::string>
registerFiles(FileManager &files, const CompilationDatabase &database,
              const std::vector<std::string> &sources) {
  auto imported = discoverCompilationFiles(database, sources)
                      .and_then([&files](std::vector<std::string> paths) {
                        return files.addBulk(paths);
                      });
  if (!imported) {
    return std::unexpected("cannot pre-import files: " +
                           imported.error().message());
  }
  return {};
}

std::expected<int, std::string> extract(const cli::ExtractOptions &options,
                                        CompilationDatabasePtr database) {
  FileManager files(options.configuration);
  auto sources = selectSources(*database, options.sources);
  return registerFiles(files, *database, sources).transform([&] {
    clang::tooling::ClangTool tool(*database, sources);
    addPlatformFlags(tool);

    FactStore store(options.output);
    IndexingStatus indexing;
    store.begin();
    const auto toolResult =
        tool.run(createFactExtractorFactory(files, store, indexing).get());
    store.end();
    return toolResult != 0 ? toolResult : indexing.complete() ? 0 : 1;
  });
}

} // namespace

std::expected<int, std::string> runExtract(const cli::ExtractOptions &options) {
  return validateDatabasePaths(options)
      .and_then(
          [&] { return loadStoredCompilationDatabase(options.configuration); })
      .and_then(requireStoredCommands)
      .and_then([&](CompilationDatabasePtr database) {
        return extract(options, std::move(database));
      });
}

} // namespace facts::commands
