#ifndef FACTS_TOOL_AST_EXTRACTORS_FUNCTIONDECL_H
#define FACTS_TOOL_AST_EXTRACTORS_FUNCTIONDECL_H

#include "ast/extractors/Extraction.h"
#include "model/Function.h"

namespace clang {
class FunctionDecl;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

ExtractionResult<Function>
extractFunction(const clang::FunctionDecl &node,
                const clang::SourceManager &sourceManager, FileManager &files,
                FactStore &store);

} // namespace facts

#endif
