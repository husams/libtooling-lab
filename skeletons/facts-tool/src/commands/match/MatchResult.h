#pragma once

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <llvm/Support/JSON.h>

#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace facts::cli {
struct MatchOptions;
}

namespace facts::commands::match {

llvm::json::Object describeMatch(
    const clang::ast_matchers::MatchFinder::MatchResult &result,
    const std::optional<std::string> &relationKind);

std::string describeLocation(const clang::NamedDecl &node,
                             const clang::ASTContext &context);
std::string describeLocation(const clang::Stmt &node,
                             const clang::ASTContext &context);

void writeResults(const cli::MatchOptions &options,
                  const std::vector<std::string> &sources,
                  llvm::json::Array matches, const std::string &text,
                  std::ostream &output);

} // namespace facts::commands::match
