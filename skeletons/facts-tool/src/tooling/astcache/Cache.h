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

// Preprocessing commands can reuse an existing AST without requiring a source
// to be semantically valid on a cache miss.
std::optional<IncludeGraphFacts>
cachedIncludes(const clang::tooling::CompilationDatabase &database,
               const std::string &source, const Options &options);

} // namespace facts::astcache
