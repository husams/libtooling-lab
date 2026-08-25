#include "commands/Extract.h"

#include "commands/DatabasePaths.h"

#include "ast/FactExtractor.h"
#include "ast/Indexing.h"
#include "platform/PlatformFlags.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"
#include "tooling/CompilationFiles.h"
#include "tooling/StoredCompilationDatabase.h"

#include <clang/Tooling/Tooling.h>

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace facts::commands {
namespace {

using CompilationDatabase = clang::tooling::CompilationDatabase;
using CompilationDatabasePtr = std::unique_ptr<CompilationDatabase>;
using TimingClock = std::chrono::steady_clock;

bool timingsEnabled() {
  const auto *value = std::getenv("FACTS_TOOL_TIMING");
  return value != nullptr && std::string_view(value) != "0";
}

void reportTiming(std::string_view phase, TimingClock::time_point started) {
  if (!timingsEnabled()) {
    return;
  }
  const auto elapsed =
      std::chrono::duration<double, std::milli>(TimingClock::now() - started);
  std::cerr << "facts-tool timing: " << phase << ": " << elapsed.count()
            << " ms\n";
}

template <typename Operation>
decltype(auto) timePhase(std::string_view phase, Operation &&operation) {
  if (!timingsEnabled()) {
    return std::invoke(std::forward<Operation>(operation));
  }

  const auto started = TimingClock::now();
  if constexpr (std::is_void_v<std::invoke_result_t<Operation &&>>) {
    std::invoke(std::forward<Operation>(operation));
    reportTiming(phase, started);
  } else {
    decltype(auto) result = std::invoke(std::forward<Operation>(operation));
    reportTiming(phase, started);
    return result;
  }
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
  auto imported = timePhase("discover compilation files", [&] {
                    return discoverCompilationFiles(database, sources);
                  }).and_then([&files](std::vector<std::string> paths) {
    return timePhase("register compilation files",
                     [&] { return files.addBulk(paths); });
  });
  if (!imported) {
    return std::unexpected("cannot pre-import files: " +
                           imported.error().message());
  }
  return {};
}

std::expected<int, std::string> extract(const cli::ExtractOptions &options,
                                        CompilationDatabasePtr database) {
  const auto openProjectStarted = TimingClock::now();
  FileManager files(options.configuration);
  reportTiming("open project database", openProjectStarted);
  auto sources = timePhase("select sources", [&] {
    return selectSources(*database, options.sources);
  });
  return registerFiles(files, *database, sources).and_then([&] {
    const auto configureToolStarted = TimingClock::now();
    auto configured = configurePlatformCompilationDatabase(*database, sources);
    if (!configured) {
      return std::expected<int, std::string>{
          std::unexpected(configured.error())};
    }
    clang::tooling::ClangTool tool(**configured, sources);
    reportTiming("configure Clang tool", configureToolStarted);

    const auto openOutputStarted = TimingClock::now();
    FactStore store(options.output);
    reportTiming("open output database", openOutputStarted);
    IndexingStatus indexing;
    auto started =
        timePhase("begin output transaction", [&] { return store.begin(); });
    if (!started) {
      return std::expected<int, std::string>{std::unexpected(
          "cannot begin output transaction: " + started.error().message())};
    }
    const auto toolResult = timePhase("Clang parse and AST extraction", [&] {
      return tool.run(createFactExtractorFactory(files, store, indexing).get());
    });
    const auto result = toolResult != 0       ? toolResult
                        : indexing.complete() ? 0
                                              : 1;
    auto finished = result == 0 ? timePhase("commit output transaction",
                                            [&] { return store.end(); })
                                : timePhase("rollback output transaction",
                                            [&] { return store.rollback(); });
    if (!finished) {
      return std::expected<int, std::string>{std::unexpected(
          "cannot finish output transaction: " + finished.error().message())};
    }
    return std::expected<int, std::string>{result};
  });
}

} // namespace

std::expected<int, std::string> runExtract(const cli::ExtractOptions &options) {
  return timePhase("validate database paths",
                   [&] {
                     return validateDatabasePaths(options.output,
                                                  options.configuration);
                   })
      .and_then([&] {
        return timePhase("load compilation database", [&] {
          return loadStoredCompilationDatabase(options.configuration);
        });
      })
      .and_then([](CompilationDatabasePtr database) {
        return timePhase("validate stored commands", [&] {
          return requireStoredCommands(std::move(database));
        });
      })
      .and_then([&](CompilationDatabasePtr database) {
        return timePhase("extract total",
                         [&] { return extract(options, std::move(database)); });
      });
}

} // namespace facts::commands
