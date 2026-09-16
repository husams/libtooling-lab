#pragma once

#include "ast/visitors/IncludeVisitor.h"
#include "tooling/astcache/Options.h"

#include <string>

namespace clang::tooling {
class CompilationDatabase;
}

namespace facts::astcache {

// Executes one preprocessing pass and publishes dependency metadata only after
// successful completion. Disabled caching leaves the project database alone.
int preprocess(const clang::tooling::CompilationDatabase &database,
               const std::string &source, IncludeGraphFacts &includes,
               const Options &options);

} // namespace facts::astcache
