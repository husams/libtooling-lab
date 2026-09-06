#pragma once

#include "analysis/callgraph/CallGraphTypes.h"
#include "ast/extractors/Extraction.h"

#include <vector>

namespace clang {
class ASTContext;
class FunctionDecl;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

ExtractionResult<std::vector<callgraph::CallFact>>
extractDestructorCalls(const clang::FunctionDecl &caller,
                       clang::ASTContext &context, FileManager &files,
                       FactStore &store);

} // namespace facts
