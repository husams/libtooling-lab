#include "commands/match/ParsedMatcher.h"
#include <clang/ASTMatchers/Dynamic/Diagnostics.h>
#include <clang/ASTMatchers/Dynamic/Parser.h>

namespace facts::commands::match {
std::expected<ParsedMatcher, std::string>
parseMatcher(const cli::MatchOptions &options, BindingPolicy policy) {
  clang::ast_matchers::dynamic::Diagnostics diagnostics;
  llvm::StringRef expression(options.matcher);
  auto matcher = clang::ast_matchers::dynamic::Parser::parseMatcherExpression(
      expression, &diagnostics);
  if (!matcher)
    return std::unexpected("invalid matcher: " + diagnostics.toString());
  if (policy == BindingPolicy::Contract || options.relationKind)
    return ParsedMatcher{std::move(*matcher), {}};
  std::string root = "__facts_root";
  while (options.matcher.contains(root)) root += '_';
  matcher->setAllowBind(true);
  auto bound = matcher->tryBind(root);
  if (!bound) return std::unexpected("matcher cannot bind its result node");
  return ParsedMatcher{std::move(*bound), std::move(root)};
}
}
