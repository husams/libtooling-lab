#pragma once

#include "ast/visitors/IncludeVisitor.h"
#include "tooling/astcache/Options.h"

#include <expected>
#include <string>

namespace clang::tooling {
class CompilationDatabase;
}

namespace facts::commands {
// The database already includes platform flags. On a miss, preserve the
// preprocessing-only contract: import/dependency also accept incomplete C++.
std::expected<IncludeGraphFacts, std::string> preprocessTranslationUnit(
    const clang::tooling::CompilationDatabase &database,
    const std::string &source, const astcache::Options &cache);
}
