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
// Enabled caches also append their dependency graph when includes is supplied;
// hits reuse the validated database snapshot without inspecting the AST again.
int buildASTs(const clang::tooling::CompilationDatabase &database,
              const std::vector<std::string> &sources,
              std::vector<std::unique_ptr<clang::ASTUnit>> &units,
              const Options &options,
              clang::DiagnosticConsumer *diagnostics = nullptr,
              bool clearAdjusters = false,
              IncludeGraphFacts *includes = nullptr);

// Materialize the AST and dependency snapshot during import. Current entries
// only validate their artifact and return persisted dependencies, without
// deserializing the AST. Preprocessable sources with semantic errors retain
// dependency-only import support.
int prepareAST(const clang::tooling::CompilationDatabase &database,
               const std::string &source, IncludeGraphFacts &includes,
               const Options &options);

// Reconstruct preprocessing metadata from the project database. This does not
// open or deserialize the optional AST artifact.
std::optional<IncludeGraphFacts>
cachedIncludes(const clang::tooling::CompilationDatabase &database,
               const std::string &source, const Options &options);

} // namespace facts::astcache
