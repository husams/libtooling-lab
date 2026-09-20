#include "commands/match/MatchCallback.h"

#include "commands/match/ContractMatch.h"
#include "commands/match/GenericMatch.h"
#include "commands/match/MatchResult.h"

#include <iostream>
#include <iterator>

namespace facts::commands::match {

MatchCallback::MatchCallback(const cli::MatchOptions &options,
                             FileManager &files, FactStore &store,
                             bool rejectLegacyWrites, BindingPolicy policy,
                             std::string internalRoot)
    : options_(options), files_(files), store_(store),
      rejectLegacyWrites_(rejectLegacyWrites), policy_(policy),
      internalRoot_(std::move(internalRoot)) {}

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
  auto persisted = policy_ == BindingPolicy::Any && !options_.relationKind
      ? persistBindings(result, options_, internalRoot_, files_, store_, fingerprints_)
      : persistContract(result, options_, files_, store_, fingerprints_, text_, internalRoot_);
  if (!persisted)
    error_ = persisted.error();
  else {
    if (options_.format == "json")
      results_.push_back(describeMatch(result, options_,
                                        internalRoot_, policy_));
    matches_.insert(matches_.end(), std::make_move_iterator(persisted->begin()),
                    std::make_move_iterator(persisted->end()));
  }
}

MatchOutput MatchCallback::takeOutput(const std::vector<std::string> &sources) {
  return describeResults(options_, sources, std::move(results_), text_.str());
}

} // namespace facts::commands::match
