#pragma once

#include "analysis/callgraph/CallGraphTypes.h"
#include "ast/extractors/Extraction.h"

#include <optional>

namespace clang {
class Expr;
class FunctionDecl;
class SourceManager;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

ExtractionResult<std::optional<callgraph::CallFact>>
extractCallSite(const clang::FunctionDecl &caller,
                const clang::FunctionDecl &callee, const clang::Expr &site,
                const clang::SourceManager &sourceManager, FileManager &files,
                FactStore &store);

} // namespace facts
