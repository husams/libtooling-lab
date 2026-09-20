#pragma once
#include "cli/Options.h"
#include "commands/match/ExpressionEvidence.h"
#include "model/MatchedSymbol.h"
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <iosfwd>

namespace facts::commands::match {
std::expected<std::vector<MatchedSymbol>, std::string>
persistContract(const clang::ast_matchers::MatchFinder::MatchResult &result,
                 const cli::MatchOptions &options, FileManager &files,
                 FactStore &store, SourceFingerprintCache &fingerprints,
                 std::ostream &text);
}
