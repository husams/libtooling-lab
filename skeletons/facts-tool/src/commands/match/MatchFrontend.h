#pragma once

#include "ast/visitors/IncludeVisitor.h"

#include <cstddef>
#include <string>

namespace clang::ast_matchers {
class MatchFinder;
}

namespace clang::tooling {
class CompilationDatabase;
}

namespace facts::commands::match {

struct MatchFrontendResult {
  int status = 0;
  IncludeGraphFacts includes;
};

MatchFrontendResult runTranslationUnit(
    const clang::tooling::CompilationDatabase &database,
    clang::ast_matchers::MatchFinder &finder, const std::string &source,
    std::size_t index, std::size_t total);

} // namespace facts::commands::match
