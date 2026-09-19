#include "commands/match/MatchCallback.h"

#include "commands/match/DirectCall.h"
#include "commands/match/MatchContract.h"
#include "commands/match/MatchResult.h"
#include "commands/match/MatchResultLocation.h"
#include "commands/match/RelationPersistence.h"
#include "commands/match/SymbolDispatch.h"
#include "cli/Verbose.h"

#include <iostream>
#include <iterator>
#include <type_traits>
#include <variant>

namespace facts::commands::match {

MatchCallback::MatchCallback(const cli::MatchOptions &options,
                             FileManager &files, FactStore &store,
                             bool rejectLegacyWrites,
                             std::string implicitRootBinding)
    : options_(options), files_(files), store_(store),
      rejectLegacyWrites_(rejectLegacyWrites),
      implicitRootBinding_(std::move(implicitRootBinding)) {}

std::optional<clang::TraversalKind>
MatchCallback::getCheckTraversalKind() const {
  if (!options_.traversal)
    return std::nullopt;
  if (*options_.traversal == "AsIs")
    return clang::TK_AsIs;
  return clang::TK_IgnoreUnlessSpelledInSource;
}

void MatchCallback::run(
    const clang::ast_matchers::MatchFinder::MatchResult &result) {
  if (error_)
    return;
  if (rejectLegacyWrites_) {
    error_ = "incompatible-symbol-universe: facts store has no provenance; "
             "write to a new facts file and extract every source";
    return;
  }
  auto bindings = result.Nodes.getMap();
  if (!implicitRootBinding_.empty()) {
    if (auto root = bindings.extract(implicitRootBinding_);
        !root.empty() && bindings.empty()) {
      root.key() = "root";
      bindings.insert(std::move(root));
    }
  }
  auto contracts = classify(bindings, options_.relationKind,
                            {options_.sourceBinding, options_.targetBinding,
                             options_.siteBinding, options_.callBinding,
                             options_.calleeBinding});
  if (!contracts) {
    error_ = contracts.error();
    return;
  }
  if (contracts->empty() && options_.format == "text")
    text_ << "match bindings=0 source="
          << sourcePath(*result.SourceManager,
                        result.SourceManager->getLocForStartOfFile(
                            result.SourceManager->getMainFileID()))
          << '\n';
  for (const auto &contract : *contracts) {
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
                  if (options_.format == "text")
                    text_ << "symbol kind=" << symbol.kind
                          << " name=" << symbol.name
                          << describeLocation(match.symbol, *result.Context)
                          << '\n';
                  std::vector<MatchedSymbol> matched;
                  appendMatchedIndex(matched, std::move(symbol));
                  return matched;
                });
          } else if constexpr (std::is_same_v<Value, ExpressionMatch>) {
            auto captured = captureExpression(match.expression, *result.Context,
                                              files_, store_, fingerprints_);
            if (captured) {
              cli::logVerbose(options_.verbosity, 3,
                              "facts-tool: match: expression captured");
              if (options_.format == "text")
                text_ << "expression kind=" << match.expression.getStmtClassName()
                      << describeLocation(match.expression, *result.Context)
                      << '\n';
            }
            return captured.transform([] { return std::vector<MatchedSymbol>{}; });
          } else if constexpr (std::is_same_v<Value, RelationMatch>) {
            return persistRelation(match, *result.Context, files_, store_, text_);
          } else if constexpr (std::is_same_v<Value, DirectCallMatch>) {
            return persistDirectCall(match, *result.Context, files_, store_, text_,
                                     options_.format == "text");
          } else {
            if (options_.format == "text")
              text_ << "binding name=" << match.binding
                    << " kind=" << match.node.getNodeKind().asStringRef().str()
                    << describeLocation(match.node, *result.Context) << '\n';
            return std::vector<MatchedSymbol>{};
          }
        },
        contract);
    if (!persisted) {
      error_ = persisted.error();
      return;
    } else {
      matches_.insert(matches_.end(), std::make_move_iterator(persisted->begin()),
                      std::make_move_iterator(persisted->end()));
    }
  }
  if (options_.format == "json")
    results_.push_back(describeMatch(result, options_.relationKind, bindings));
}

void MatchCallback::writeResults(const std::vector<std::string> &sources,
                                 std::ostream &output) {
  match::writeResults(options_, sources, std::move(results_), text_.str(), output);
}

} // namespace facts::commands::match
