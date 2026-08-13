#ifndef FACTS_TOOL_AST_EXTRACTORS_FUNCTIONDECL_H
#define FACTS_TOOL_AST_EXTRACTORS_FUNCTIONDECL_H

#include "ast/extractors/Symbol.h"
#include "model/Function.h"

namespace clang {
class FunctionDecl;
} // namespace clang

namespace facts {

template <>
ExtractionResult<Function> extractSymbol<Function, clang::FunctionDecl>(
    const clang::FunctionDecl &node, const clang::SourceManager &sourceManager);

} // namespace facts

#endif
