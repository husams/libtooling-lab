#include "commands/match/MatchExecution.h"

#include "commands/FactPairValidation.h"
#include "commands/ExtractionSetup.h"
#include "commands/match/MatchCallback.h"
#include "commands/match/MatchCancellation.h"
#include "commands/match/MatchFrontend.h"
#include "commands/match/MatchPublication.h"
#include "commands/match/MatchPreparation.h"
#include "commands/match/ParsedMatcher.h"
#include "platform/PlatformFlags.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

#include <exception>
#include <filesystem>
#include <optional>
#include <utility>

namespace facts::commands::match {
using Result = std::expected<MatchOutput, std::string>;

Result execute(const cli::MatchOptions &options,
               CompilationDatabasePtr database, FileManager &files,
               const std::vector<std::string> &sources,
               const std::string &fingerprint, bool installSignals,
               BindingPolicy policy) {
  auto configured = configurePlatformCompilationDatabase(*database, sources, options.astCache);
  if (!configured)
    return std::unexpected("cannot configure translation units: " +
                           configured.error());
  auto matcher = parseMatcher(options, policy);
  if (!matcher) return std::unexpected(matcher.error());
  auto prepared = prepare(options, *database, files, sources, fingerprint, !installSignals);
  if (!prepared) return std::unexpected(prepared.error());
  const auto &selected = prepared->selected;
  const auto &pairing = prepared->pairing;
  try {
    std::optional<MatchCancellation> cancellation;
    if (installSignals) cancellation.emplace();
    FactStore store(options.facts, options.verbosity);
    if (auto begun = store.begin(); !begun)
      return std::unexpected("cannot begin facts transaction: " +
                             begun.error().message());
    if (pairing) {
      auto refreshed = store.refreshCloneFiles(*pairing, selected);
      if (!refreshed) {
        (void)store.rollback();
        return std::unexpected("cannot refresh clone match facts: " +
                               refreshed.error().message());
      }
    }
    MatchCallback callback(options, files, store, prepared->rejectLegacyWrites,
                             policy, matcher->internalRoot);
    clang::ast_matchers::MatchFinder finder;
    if (!finder.addDynamicMatcher(matcher->matcher, &callback))
      return finishMatch(store, options, 1,
                         "matcher cannot run at the top level", {})
          .transform([](int) { return MatchOutput{}; });
    for (std::size_t index = 0; index < sources.size(); ++index) {
      const auto &source = sources[index];
      auto frontend = runTranslationUnit(**configured, finder, source, index,
                                          sources.size(), options.astCache);
      auto registered = requireRegisteredFiles(
          files, frontend.includes.visitedSources, fingerprint);
      if (!registered) {
        return finishMatch(store, options, 1, registered.error(),
                           callback.matchedSymbols(), selected,
                           pairing ? &*pairing : nullptr)
            .transform([](int) { return MatchOutput{}; });
      }
      if (installSignals && MatchCancellation::cancelled()) {
        return finishMatch(store, options, frontend.status,
                           "facts-tool: cancelled during match",
                           callback.matchedSymbols(), selected,
                           pairing ? &*pairing : nullptr)
            .transform([](int) { return MatchOutput{}; });
      }
      if (callback.error() || frontend.status != 0) {
        return finishMatch(store, options, frontend.status, callback.error(),
                           callback.matchedSymbols(), selected,
                           pairing ? &*pairing : nullptr)
            .transform([](int) { return MatchOutput{}; });
      }
    }
    return finishMatch(store, options, 0, std::nullopt,
                       callback.matchedSymbols(), selected,
                       pairing ? &*pairing : nullptr)
        .transform([&](int) { return callback.takeOutput(sources); });
  } catch (const std::exception &error) {
    return std::unexpected("cannot persist match evidence: " +
                           std::string{error.what()});
  }
}

} // namespace facts::commands::match
