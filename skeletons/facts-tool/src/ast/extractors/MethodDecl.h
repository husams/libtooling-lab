#ifndef FACTS_TOOL_AST_EXTRACTORS_METHODDECL_H
#define FACTS_TOOL_AST_EXTRACTORS_METHODDECL_H

#include "ast/Indexing.h"
#include "ast/extractors/Extraction.h"
#include "model/Function.h"

#include <expected>
#include <system_error>

namespace clang {
class FunctionDecl;
class SourceManager;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

ExtractionResult<Function> addMethodFlags(Function function,
                                          const clang::FunctionDecl &node);

IndexingResult storeMethodRelation(const clang::FunctionDecl &node,
                                   SymbolId function,
                                   const clang::SourceManager &sourceManager,
                                   FileManager &files, FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_METHODDECL_H
