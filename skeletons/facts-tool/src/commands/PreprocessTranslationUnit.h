#pragma once

#include "ast/visitors/IncludeVisitor.h"
#include "tooling/astcache/Options.h"

#include <expected>
#include <string>

namespace clang::tooling {
class CompilationDatabase;
}

namespace facts::commands {
// The database already includes platform flags. Reuse current project metadata
// or collect it during preprocessing; this does not require valid C++ semantics.
std::expected<IncludeGraphFacts, std::string> preprocessTranslationUnit(
    const clang::tooling::CompilationDatabase &database,
    const std::string &source, const astcache::Options &cache);
}
