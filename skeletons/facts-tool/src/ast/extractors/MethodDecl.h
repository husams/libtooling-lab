#ifndef FACTS_TOOL_AST_EXTRACTORS_METHODDECL_H
#define FACTS_TOOL_AST_EXTRACTORS_METHODDECL_H

#include "ast/Indexing.h"
#include "ast/extractors/Extraction.h"
#include "model/Function.h"

#include <expected>
#include <system_error>

namespace clang {
class FunctionDecl;
} // namespace clang

namespace facts {
class FactStore;

ExtractionResult<Function> addMethodFlags(Function function,
                                          const clang::FunctionDecl &node);

IndexingResult storeMethodRelation(const clang::FunctionDecl &node,
                                   SymbolId function, FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_METHODDECL_H
