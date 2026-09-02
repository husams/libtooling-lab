#include "commands/match/MatchCallback.h"

#include "commands/match/DirectCall.h"
#include "commands/match/MatchContract.h"
#include "commands/match/RelationPersistence.h"
#include "commands/match/SymbolDispatch.h"

#include <iostream>
#include <type_traits>
#include <variant>

namespace facts::commands::match {

MatchCallback::MatchCallback(const cli::MatchOptions &options,
                             FileManager &files, FactStore &store)
    : options_(options), files_(files), store_(store) {}

void MatchCallback::run(
    const clang::ast_matchers::MatchFinder::MatchResult &result) {
  if (error_)
    return;
  auto contract = classify(result.Nodes, options_.relationKind);
  if (!contract) {
    error_ = contract.error();
    return;
  }
  auto persisted = std::visit(
      [&](auto match) -> std::expected<void, std::string> {
        using Value = decltype(match);
        if constexpr (std::is_same_v<Value, SymbolMatch>) {
          return persistSymbol(match.symbol, *result.Context, files_, store_)
              .transform([](PersistedSymbol symbol) {
                std::cout << "symbol kind=" << symbol.kind
                          << " name=" << symbol.name << '\n';
              });
        } else if constexpr (std::is_same_v<Value, RelationMatch>) {
          return persistRelation(match, *result.Context, files_, store_);
        } else {
          return persistDirectCall(match, *result.Context, files_, store_);
        }
      },
      *contract);
  if (!persisted)
    error_ = persisted.error();
}

} // namespace facts::commands::match
