#include "commands/Match.h"

#include "commands/ExtractionSetup.h"
#include "commands/match/MatchCallback.h"
#include "commands/match/RelationKinds.h"
#include "platform/PlatformFlags.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"
#include "tooling/StoredCompilationDatabase.h"

#include <clang/ASTMatchers/Dynamic/Diagnostics.h>
#include <clang/ASTMatchers/Dynamic/Parser.h>
#include <clang/Tooling/Tooling.h>

namespace facts::commands {
namespace {
using Result = std::expected<int, std::string>;

Result finish(FactStore &store, int status, std::optional<std::string> error) {
  auto finished = status == 0 && !error ? store.end() : store.rollback();
  if (!finished)
    return std::unexpected("cannot finish facts transaction: " +
                           finished.error().message());
  if (error)
    return std::unexpected(*error);
  return status == 0 ? Result{0}
                     : Result{std::unexpected(
                           "translation unit matching failed")};
}

Result run(const cli::MatchOptions &options, CompilationDatabasePtr database,
           FileManager &files, const std::vector<std::string> &sources) {
  auto configured = configurePlatformCompilationDatabase(*database, sources);
  if (!configured)
    return std::unexpected("cannot configure translation units: " +
                           configured.error());
  clang::tooling::ClangTool tool(**configured, sources);
  clang::ast_matchers::dynamic::Diagnostics diagnostics;
  llvm::StringRef expression(options.matcher);
  auto matcher = clang::ast_matchers::dynamic::Parser::parseMatcherExpression(
      expression, &diagnostics);
  if (!matcher)
    return std::unexpected("invalid matcher: " + diagnostics.toString());
  FactStore store(options.facts, options.verbosity);
  if (auto begun = store.begin(); !begun)
    return std::unexpected("cannot begin facts transaction: " +
                           begun.error().message());
  match::MatchCallback callback(options, files, store);
  clang::ast_matchers::MatchFinder finder;
  if (!finder.addDynamicMatcher(*matcher, &callback))
    return finish(store, 1, "matcher cannot run at the top level");
  const auto status =
      tool.run(clang::tooling::newFrontendActionFactory(&finder).get());
  return finish(store, status, callback.error());
}
} // namespace

std::expected<int, std::string> runMatch(const cli::MatchOptions &options) {
  if (options.relationKind) {
    auto kind = match::parseRelationKind(*options.relationKind);
    if (!kind)
      return std::unexpected(kind.error());
  }
  auto loaded = loadStoredCompilationDatabase(options.facts, options.sources);
  if (!loaded)
    return std::unexpected("cannot load project configuration: " +
                           loaded.error());
  auto commands = requireStoredCommands(std::move(*loaded));
  if (!commands)
    return std::unexpected(commands.error());
  auto opened = FileManager::openReadOnly(options.facts, options.verbosity);
  if (!opened)
    return std::unexpected(opened.error());
  auto registry = requireCompletedRegistry(**opened);
  if (!registry)
    return std::unexpected(registry.error());
  auto sources = selectSources(**commands, options.sources);
  auto registered =
      requireRegisteredSources(**opened, **commands, sources, *registry);
  if (!registered)
    return std::unexpected(registered.error());
  return run(options, std::move(*commands), **opened, sources);
}

} // namespace facts::commands
