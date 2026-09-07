#include "commands/match/MatchCallback.h"

#include "commands/match/DirectCall.h"
#include "commands/match/MatchContract.h"
#include "commands/match/RelationPersistence.h"
#include "commands/match/SymbolDispatch.h"

#include <iostream>
#include <iterator>
#include <type_traits>
#include <variant>

namespace facts::commands::match {

MatchCallback::MatchCallback(const cli::MatchOptions &options,
                             FileManager &files, FactStore &store,
                             bool rejectLegacyWrites)
    : options_(options), files_(files), store_(store),
      rejectLegacyWrites_(rejectLegacyWrites) {}

void MatchCallback::run(
    const clang::ast_matchers::MatchFinder::MatchResult &result) {
  if (error_)
    return;
  if (rejectLegacyWrites_) {
    error_ = "incompatible-symbol-universe: facts store has no provenance; "
             "write to a new facts file and extract every source";
    return;
  }
  auto contract = classify(result.Nodes, options_.relationKind);
  if (!contract) {
    error_ = contract.error();
    return;
  }
  auto persisted = std::visit(
      [&](auto match)
          -> std::expected<std::vector<MatchedSymbol>, std::string> {
        using Value = decltype(match);
        if constexpr (std::is_same_v<Value, SymbolMatch>) {
          return persistSymbol(match.symbol, *result.Context, files_, store_)
              .and_then([&](PersistedSymbol symbol) {
                if (!options_.captureSource)
                  return std::expected<PersistedSymbol, std::string>{
                      std::move(symbol)};
                return captureSourceRegion(match.symbol, symbol.id,
                                           *result.Context, files_, store_,
                                           fingerprints_)
                    .transform([&] { return std::move(symbol); });
              })
              .transform([&](PersistedSymbol symbol) {
                std::cout << "symbol kind=" << symbol.kind
                          << " name=" << symbol.name << '\n';
                std::vector<MatchedSymbol> matched;
                appendMatchedIndex(matched, std::move(symbol));
                return matched;
              });
        } else if constexpr (std::is_same_v<Value, ExpressionMatch>) {
          return captureExpression(match.expression, *result.Context, files_,
                                   store_, fingerprints_)
              .transform([] { return std::vector<MatchedSymbol>{}; });
        } else if constexpr (std::is_same_v<Value, RelationMatch>) {
          return persistRelation(match, *result.Context, files_, store_);
        } else {
          return persistDirectCall(match, *result.Context, files_, store_);
        }
      },
      *contract);
  if (!persisted)
    error_ = persisted.error();
  else
    matches_.insert(matches_.end(), std::make_move_iterator(persisted->begin()),
                    std::make_move_iterator(persisted->end()));
}

} // namespace facts::commands::match
