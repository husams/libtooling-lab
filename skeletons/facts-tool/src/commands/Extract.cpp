#include "commands/Extract.h"

#include "commands/CompilationDatabase.h"
#include "commands/DatabasePaths.h"
#include "commands/ExtractionSetup.h"

#include "ast/FactExtractor.h"
#include "ast/Indexing.h"
#include "cli/Verbose.h"
#include "platform/PlatformFlags.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"
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

template <typename Operation>
decltype(auto) runExtractStage(const cli::ExtractOptions &options,
                               std::string_view stage, Operation &&operation) {
  return cli::runStage(options.verbosity, "extract", stage, [&] {
    return timePhase(stage, std::forward<Operation>(operation));
  });
}

std::expected<int, std::string> extract(const cli::ExtractOptions &options,
                                        CompilationDatabasePtr database) {
  auto opened = runExtractStage(options, "open project database", [&] {
    return FileManager::openReadOnly(options.configuration, options.verbosity);
  });
  if (!opened) {
    return std::expected<int, std::string>{std::unexpected(opened.error())};
  }
  auto &files = **opened;
  auto registry =
      runExtractStage(options, "validate registry completeness",
                      [&] { return requireCompletedRegistry(files); });
  if (!registry) {
    return std::expected<int, std::string>{std::unexpected(registry.error())};
  }
  auto sources = runExtractStage(options, "select sources", [&] {
    return selectSources(*database, options.sources);
  });
  cli::logVerbose(options.verbosity, 2,
                  "facts-tool: extract: selected_sources={}", sources.size());
  return runExtractStage(options, "resolve registered sources",
                         [&] {
                           return requireRegisteredSources(files, *database,
                                                           sources, *registry);
                         })
      .and_then([&] {
        auto configured = runExtractStage(options, "configure Clang tool", [&] {
          return configurePlatformCompilationDatabase(*database, sources);
        });
        if (!configured) {
          return std::expected<int, std::string>{
              std::unexpected(configured.error())};
        }
        clang::tooling::ClangTool tool(**configured, sources);

        cli::logVerbose(options.verbosity, 1,
                        "facts-tool: extract: open output database");
        const auto openOutputStarted = TimingClock::now();
        FactStore store(options.output, options.verbosity);
        reportTiming("open output database", openOutputStarted);
        IndexingStatus indexing;
        auto started = runExtractStage(options, "begin output transaction",
                                       [&] { return store.begin(); });
        if (!started) {
          return std::expected<int, std::string>{std::unexpected(
              "cannot begin output transaction: " + started.error().message())};
        }
        const auto toolResult =
            runExtractStage(options, "Clang parse and AST extraction", [&] {
              return tool.run(
                  createFactExtractorFactory(files, store, indexing).get());
            });
        const auto result = toolResult != 0       ? toolResult
                            : indexing.complete() ? 0
                                                  : 1;
        auto finished =
            result == 0
                ? runExtractStage(options, "commit output transaction",
                                  [&] { return store.end(); })
                : runExtractStage(options, "rollback output transaction",
                                  [&] { return store.rollback(); });
        if (!finished) {
          return std::expected<int, std::string>{
              std::unexpected("cannot finish output transaction: " +
                              finished.error().message())};
        }
        return std::expected<int, std::string>{result};
      });
}

} // namespace

std::expected<int, std::string> runExtract(const cli::ExtractOptions &options) {
  return runExtractStage(options, "validate database paths",
                         [&] {
                           return validateDatabasePaths(options.output,
                                                        options.configuration);
                         })
      .and_then([&] {
        return runExtractStage(options, "load compilation database", [&] {
          return loadStoredCompilationDatabase(options.configuration,
                                               options.sources);
        });
      })
      .transform([&](CompilationDatabasePtr database) {
        return appendExtraArguments(std::move(database),
                                    options.extraArguments);
      })
      .and_then([&](CompilationDatabasePtr database) {
        return runExtractStage(options, "validate stored commands", [&] {
          return requireStoredCommands(std::move(database));
        });
      })
      .and_then([&](CompilationDatabasePtr database) {
        return cli::runStage(options.verbosity, "extract", "extract facts",
                             [&] {
                               return timePhase("extract total", [&] {
                                 return extract(options, std::move(database));
                               });
                             });
      });
}

} // namespace facts::commands
