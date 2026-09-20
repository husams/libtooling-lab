#include "commands/match/GenericMatch.h"
#include "commands/match/GenericSymbol.h"
#include "commands/match/VisibleBindings.h"

namespace facts::commands::match {
std::expected<std::vector<MatchedSymbol>, std::string>
persistBindings(const clang::ast_matchers::MatchFinder::MatchResult &result,
                 const cli::MatchOptions &options, std::string_view internalRoot,
                 FileManager &files, FactStore &store,
                 SourceFingerprintCache &fingerprints) {
  std::vector<MatchedSymbol> symbols;
  for (const auto &[name, node] : visibleBindings(result.Nodes, internalRoot)) {
    if (const auto *declaration = node.get<clang::NamedDecl>()) {
      auto symbol = persistAnySymbol(*declaration, *result.Context, files, store);
      if (!symbol) return std::unexpected(symbol.error());
      if (!*symbol) continue;
      if (options.captureSource) {
        auto captured = captureSourceRegion(*declaration, (*symbol)->id,
            *result.Context, files, store, fingerprints);
        if (!captured) return std::unexpected(captured.error());
      }
      appendMatchedIndex(symbols, std::move(**symbol));
    } else if (const auto *expression = node.get<clang::Expr>();
               expression && options.captureSource) {
      auto captured = captureExpression(*expression, *result.Context,
                                         files, store, fingerprints);
      if (!captured) return std::unexpected(captured.error());
    }
  }
  return symbols;
}
}
