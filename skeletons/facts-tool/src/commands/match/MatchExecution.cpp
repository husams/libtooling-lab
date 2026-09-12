#include "commands/match/MatchExecution.h"

#include "commands/FactPairValidation.h"
#include "commands/ExtractionSetup.h"
#include "commands/match/MatchCallback.h"
#include "commands/match/MatchCancellation.h"
#include "commands/match/MatchFrontend.h"
#include "commands/match/MatchPublication.h"
#include "platform/PlatformFlags.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

#include <clang/ASTMatchers/Dynamic/Diagnostics.h>
#include <clang/ASTMatchers/Dynamic/Parser.h>

#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <utility>

namespace facts::commands::match {
using Result = std::expected<int, std::string>;

Result execute(const cli::MatchOptions &options,
               CompilationDatabasePtr database, FileManager &files,
               const std::vector<std::string> &sources,
               const std::string &fingerprint) {
  auto configured = configurePlatformCompilationDatabase(*database, sources);
  if (!configured)
    return std::unexpected("cannot configure translation units: " +
                           configured.error());
  clang::ast_matchers::dynamic::Diagnostics diagnostics;
  llvm::StringRef expression(options.matcher);
  auto matcher = clang::ast_matchers::dynamic::Parser::parseMatcherExpression(
      expression, &diagnostics);
  if (!matcher)
    return std::unexpected("invalid matcher: " + diagnostics.toString());
  std::vector<FileId> selected;
  selected.reserve(sources.size());
  for (const auto &source : sources) {
    auto id = files.getId(source);
    if (!id)
      return std::unexpected("cannot resolve match source: " +
                             id.error().message());
    selected.push_back(*id);
  }
  bool rejectLegacyWrites = false;
  std::optional<FactPairProvenanceSnapshot> pairing;
  if (!(options.factsProvided && options.facts == options.configuration)) {
    if (std::filesystem::exists(options.facts)) {
      auto legacy =
          legacyFactsNeedRegistration(options.facts, options.configuration);
      if (!legacy)
        return std::unexpected(legacy.error());
      rejectLegacyWrites = *legacy;
    }
    if (!rejectLegacyWrites) {
      auto prepared =
          prepareFactPairForWrite(options.facts, options.configuration);
      if (!prepared)
        return std::unexpected(prepared.error());
      pairing = std::move(*prepared);
    }
  }
  try {
    MatchCancellation cancellation;
    FactStore store(options.facts, options.verbosity);
    if (auto begun = store.begin(); !begun)
      return std::unexpected("cannot begin facts transaction: " +
                             begun.error().message());
    MatchCallback callback(options, files, store, rejectLegacyWrites);
    clang::ast_matchers::MatchFinder finder;
    if (!finder.addDynamicMatcher(*matcher, &callback))
      return finishMatch(store, options, 1,
                         "matcher cannot run at the top level", {});
    for (std::size_t index = 0; index < sources.size(); ++index) {
      const auto &source = sources[index];
      auto frontend = runTranslationUnit(**configured, finder, source, index,
                                          sources.size());
      auto registered = requireRegisteredFiles(
          files, frontend.includes.visitedSources, fingerprint);
      if (!registered) {
        return finishMatch(store, options, 1, registered.error(),
                           callback.matchedSymbols(), selected,
                           pairing ? &*pairing : nullptr);
      }
      if (MatchCancellation::cancelled()) {
        return finishMatch(store, options, frontend.status,
                           "facts-tool: cancelled during match",
                           callback.matchedSymbols(), selected,
                           pairing ? &*pairing : nullptr);
      }
      if (callback.error() || frontend.status != 0) {
        return finishMatch(store, options, frontend.status, callback.error(),
                           callback.matchedSymbols(), selected,
                           pairing ? &*pairing : nullptr);
      }
    }
    return finishMatch(store, options, 0, std::nullopt,
                       callback.matchedSymbols(), selected,
                       pairing ? &*pairing : nullptr)
        .transform([&](int result) {
          callback.writeResults(sources, std::cout);
          return result;
        });
  } catch (const std::exception &error) {
    return std::unexpected("cannot persist match evidence: " +
                           std::string{error.what()});
  }
}

} // namespace facts::commands::match
