#pragma once

#include "ast/visitors/IncludeVisitor.h"
#include "tooling/astcache/Options.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace clang {
class ASTUnit;
class DiagnosticConsumer;
namespace tooling {
class CompilationDatabase;
}
} // namespace clang

namespace facts::astcache {

// Appends one AST per selected compilation, retaining ClangTool's exit status.
int buildASTs(const clang::tooling::CompilationDatabase &database,
              const std::vector<std::string> &sources,
              std::vector<std::unique_ptr<clang::ASTUnit>> &units,
              const Options &options,
              clang::DiagnosticConsumer *diagnostics = nullptr,
              bool clearAdjusters = false);

// Reconstruct preprocessing metadata from the project database. This does not
// open or deserialize the optional AST artifact.
std::optional<IncludeGraphFacts>
cachedIncludes(const clang::tooling::CompilationDatabase &database,
               const std::string &source, const Options &options);

} // namespace facts::astcache
