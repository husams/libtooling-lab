#include "commands/Extract.h"

#include "commands/CompilationDatabase.h"
#include "commands/ConfigurationSupport.h"
#include "commands/DatabasePaths.h"
#include "commands/ExtraArguments.h"
#include "commands/ExtractionFreshness.h"
#include "commands/ExtractionSetup.h"
#include "commands/FactPairValidation.h"

#include "ast/FactExtractor.h"
#include "ast/Indexing.h"
#include "cli/Verbose.h"
#include "config/GitFileCommit.h"
#include "platform/PlatformFlags.h"
#include "storage/FactStore.h"
#include "storage/FileIndexState.h"
#include "storage/FileManager.h"
#include "tooling/StoredCompilationDatabase.h"

#include <clang/Tooling/Tooling.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
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

std::string utcNow() {
  return std::format("{:%FT%TZ}", std::chrono::floor<std::chrono::seconds>(
                                      std::chrono::system_clock::now()));
}

// The union of every stale TU's own transitive include set (itself
// included), in first-seen order -- everything a successful run of
// `staleSources` needs marked, without discovering it a second time.
std::vector<std::string>
filesToMark(const std::vector<std::string> &staleSources,
            const std::unordered_map<std::string, std::vector<std::string>>
                &perSource) {
  std::vector<std::string> result;
  std::unordered_set<std::string> seen;
  for (const auto &source : staleSources) {
    const auto found = perSource.find(source);
    const std::vector<std::string> singleton{source};
    const auto &closure = found != perSource.end() ? found->second : singleton;
    for (const auto &file : closure) {
      if (seen.insert(file).second) {
        result.push_back(file);
      }
    }
  }
  return result;
}

// Recording index state is an optimization a later extract can use to skip
// unchanged work, not a correctness requirement for the facts just
// committed: the facts themselves are already durably written by the time
// this runs, so every failure here -- whether the project database could
// not even be opened read-write, a file could not be resolved or stat'd, or
// the UPDATE itself failed (including a transient SQLITE_BUSY) -- is worth
// only a warning, never the exit-1 a genuine extraction failure gets.
struct RecordIndexStateError {
  std::string message;
};

// Marks every file `filesToMark` returned as indexed into `options.output`
// at the git commit each one currently resolves to. Runs after store.end()
// has already committed the facts, so a failure here must say the facts are
// safe even though the index state is not.
std::expected<void, RecordIndexStateError>
recordIndexState(const cli::ExtractOptions &options,
                 const std::vector<std::string> &files,
                 const std::unordered_map<std::string, double> &observedMtime,
                 config::GitCommitResolver &resolver) {
  std::unique_ptr<FileManager> opened;
  try {
    opened =
        std::make_unique<FileManager>(options.configuration, options.verbosity);
  } catch (const std::exception &error) {
    return std::unexpected(RecordIndexStateError{error.what()});
  }
  auto &writable = *opened;
  const auto normalizedOutput =
      std::filesystem::absolute(options.output).lexically_normal().string();
  const auto indexedAt = utcNow();

  std::vector<FileIndexRecord> records;
  records.reserve(files.size());
  for (const auto &file : files) {
    auto id = writable.getId(file);
    if (!id)
      return std::unexpected(RecordIndexStateError{
          "cannot resolve indexed file: " + file + ": " +
          id.error().message()});
    // Reuse the mtime partitionSources observed before the Clang parse ran,
    // so a file edited while a long extraction is still in flight records
    // the mtime of what was actually extracted, not whatever is on disk
    // once the parse finishes. Falls back to a fresh stat for a file that
    // was, for whatever reason, never actually observed there.
    const auto observed = observedMtime.find(file);
    const auto mtime = observed != observedMtime.end()
                           ? std::optional<double>{observed->second}
                           : currentMtime(file);
    if (!mtime)
      return std::unexpected(RecordIndexStateError{
          "cannot read the last-write time of " + file});
    records.push_back(FileIndexRecord{.id = *id,
                                      .indexedAt = indexedAt,
                                      .mtime = *mtime,
                                      .factsDb = normalizedOutput,
                                      .gitCommit = resolver.commitFor(file)});
  }
  auto marked = writable.markIndexed(records);
  if (!marked)
    return std::unexpected(RecordIndexStateError{
        "cannot record index state: " + marked.error().message()});
  return {};
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
      .and_then([&](DiscoveredIncludes discovered) {
        config::GitCommitResolver commitResolver;
        auto partitioned =
            runExtractStage(options, "check index freshness", [&] {
              return partitionSources(files, commitResolver, sources,
                                      discovered.perSource, options.output,
                                      options.force);
            });
        for (const auto &source : partitioned.upToDate) {
          cli::logVerbose(options.verbosity, 2,
                          "facts-tool: extract: skip up-to-date source={}",
                          source);
        }
        cli::logVerbose(options.verbosity, 1,
                        "facts-tool: extract: up_to_date={} stale={}",
                        partitioned.upToDate.size(), partitioned.stale.size());
        if (partitioned.stale.empty()) {
          std::cerr << "facts-tool: " << sources.size()
                    << " source(s) up to date; nothing to extract\n";
          return std::expected<int, std::string>{0};
        }
        const auto &stale = partitioned.stale;

        auto configured = runExtractStage(options, "configure Clang tool", [&] {
          return configurePlatformCompilationDatabase(*database, stale);
        });
        if (!configured) {
          return std::expected<int, std::string>{
              std::unexpected(configured.error())};
        }
        clang::tooling::ClangTool tool(**configured, stale);

        // Deferred until every applicable check above has passed, so a
        // facts_template default never creates a directory ahead of a
        // failure (B-030 C-3116).
        if (options.outputFromTemplate) {
          if (auto created = materializeFactsDirectory(options.output);
              !created)
            return std::expected<int, std::string>{
                std::unexpected(created.error())};
        }
        auto pairing =
            prepareFactPairForWrite(options.output, options.configuration);
        if (!pairing)
          return std::expected<int, std::string>{
              std::unexpected(pairing.error())};
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
        if (result == 0) {
          std::vector<FileId> selected;
          selected.reserve(stale.size());
          for (const auto &source : stale) {
            auto id = files.getId(source);
            if (!id) {
              (void)store.rollback();
              return std::expected<int, std::string>{std::unexpected(
                  "cannot resolve extracted source: " + id.error().message())};
            }
            selected.push_back(*id);
          }
          auto registered =
              registerFactPairProvenance(store, *pairing, selected);
          if (!registered) {
            (void)store.rollback();
            return std::expected<int, std::string>{
                std::unexpected(registered.error())};
          }
        }
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
        if (result != 0) {
          return std::expected<int, std::string>{result};
        }
        const auto toMark = filesToMark(stale, discovered.perSource);
        auto recorded = runExtractStage(options, "record index state", [&] {
          return recordIndexState(options, toMark, partitioned.observedMtime,
                                  commitResolver);
        });
        if (!recorded) {
          // The facts are already committed at this point; recording is
          // purely an optimization for a later extract, so any failure --
          // including a transient SQLITE_BUSY -- is a warning, not a
          // reason to fail the command that already did its real work.
          std::cerr << "facts-tool: warning: index state not recorded: "
                    << recorded.error().message << "\n";
        }
        return std::expected<int, std::string>{result};
      });
}

} // namespace

std::expected<int, std::string>
runExtractResolved(const cli::ExtractOptions &options) {
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
      .and_then([&](CompilationDatabasePtr database) {
        return mergedArguments(options.defaultExtraArguments,
                               options.extraArguments,
                               options.extraArgumentsProvided)
            .transform([&](std::vector<std::string> arguments) {
              return appendExtraArguments(std::move(database), arguments);
            });
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

std::expected<int, std::string> runExtract(const cli::ExtractOptions &options) {
  auto resolved = loadConfiguration(options.configuration,
                                    options.configurationFile, false, true);
  if (!resolved)
    return std::unexpected(resolved.error());
  auto configured = options;
  if (!configured.outputProvided) {
    auto output = resolveFactsOutput(*resolved, options.sources);
    if (!output)
      return std::unexpected(output.error());
    configured.output = output->string();
    configured.outputFromTemplate = true;
  }
  if (configured.output.empty())
    return std::unexpected(
        "facts-tool: usage error: -o/--output must not be empty");
  configured.configuration = resolved->database.string();
  configured.defaultExtraArguments = std::move(resolved->extraArguments);
  return runExtractResolved(configured);
}

} // namespace facts::commands
