#include "commands/match/ContractMatch.h"
#include "commands/match/DirectCall.h"
#include "commands/match/MatchContract.h"
#include "commands/match/MatchResult.h"
#include "commands/match/RelationPersistence.h"
#include "commands/match/SymbolDispatch.h"
#include "cli/Verbose.h"
#include <type_traits>
#include <variant>

namespace facts::commands::match {
std::expected<std::vector<MatchedSymbol>, std::string>
persistContract(const clang::ast_matchers::MatchFinder::MatchResult &result,
                 const cli::MatchOptions &options, FileManager &files,
                 FactStore &store, SourceFingerprintCache &fingerprints,
                 std::ostream &text) {
  return classify(result.Nodes, options.relationKind).and_then([&](Contract contract) {
    return std::visit([&](auto match)
        -> std::expected<std::vector<MatchedSymbol>, std::string> {
      using Value = decltype(match);
      if constexpr (std::is_same_v<Value, SymbolMatch>) {
        return persistSymbol(match.symbol, *result.Context, files, store)
            .and_then([&](PersistedSymbol symbol) {
              if (!options.captureSource)
                return std::expected<PersistedSymbol, std::string>{std::move(symbol)};
              return captureSourceRegion(match.symbol, symbol.id, *result.Context,
                                           files, store, fingerprints)
                  .transform([&] { return std::move(symbol); });
            }).transform([&](PersistedSymbol symbol) {
              if (options.format == "text")
                text << "symbol kind=" << symbol.kind << " name=" << symbol.name
                     << describeLocation(match.symbol, *result.Context) << '\n';
              std::vector<MatchedSymbol> matched;
              appendMatchedIndex(matched, std::move(symbol));
              return matched;
            });
      } else if constexpr (std::is_same_v<Value, ExpressionMatch>) {
        auto captured = captureExpression(match.expression, *result.Context,
                                            files, store, fingerprints);
        if (captured)
          cli::logVerbose(options.verbosity, 3, "facts-tool: match: expression captured");
        return captured.transform([] { return std::vector<MatchedSymbol>{}; });
      } else if constexpr (std::is_same_v<Value, RelationMatch>) {
        return persistRelation(match, *result.Context, files, store, text);
      } else {
        return persistDirectCall(match, *result.Context, files, store, text,
                                   options.format == "text");
      }
    }, contract);
  });
}
}
