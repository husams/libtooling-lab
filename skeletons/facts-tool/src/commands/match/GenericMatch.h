#pragma once
#include "cli/Options.h"
#include "commands/match/ExpressionEvidence.h"
#include "model/MatchedSymbol.h"
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <string_view>

namespace facts::commands::match {
std::expected<std::vector<MatchedSymbol>, std::string>
persistBindings(const clang::ast_matchers::MatchFinder::MatchResult &result,
                 const cli::MatchOptions &options, std::string_view internalRoot,
                 FileManager &files, FactStore &store,
                 SourceFingerprintCache &fingerprints);
}
