#include "commands/match/MatchExecution.h"

#include "commands/match/MatchCallback.h"
#include "commands/match/MatchPublication.h"
#include "platform/PlatformFlags.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

#include <clang/ASTMatchers/Dynamic/Diagnostics.h>
#include <clang/ASTMatchers/Dynamic/Parser.h>
#include <clang/Tooling/Tooling.h>

namespace facts::commands::match {
using Result = std::expected<int, std::string>;

Result execute(const cli::MatchOptions &options,
               CompilationDatabasePtr database, FileManager &files,
               const std::vector<std::string> &sources) {
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
  MatchCallback callback(options, files, store);
  clang::ast_matchers::MatchFinder finder;
  if (!finder.addDynamicMatcher(*matcher, &callback))
    return finishMatch(store, options, 1, "matcher cannot run at the top level",
                       {});
  const auto status =
      tool.run(clang::tooling::newFrontendActionFactory(&finder).get());
  return finishMatch(store, options, status, callback.error(),
                     callback.matchedSymbols());
}

} // namespace facts::commands::match
