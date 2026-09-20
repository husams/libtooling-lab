#pragma once
#include "cli/Options.h"
#include "commands/match/BindingPolicy.h"
#include <clang/ASTMatchers/ASTMatchersInternal.h>
#include <expected>

namespace facts::commands::match {
struct ParsedMatcher {
  clang::ast_matchers::internal::DynTypedMatcher matcher;
  std::string internalRoot;
};
std::expected<ParsedMatcher, std::string>
parseMatcher(const cli::MatchOptions &options, BindingPolicy policy);
}
