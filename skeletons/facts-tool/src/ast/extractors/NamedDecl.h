#ifndef FACTS_TOOL_AST_EXTRACTORS_NAMEDDECL_H
#define FACTS_TOOL_AST_EXTRACTORS_NAMEDDECL_H

#include "ast/extractors/Symbol.h"
#include "model/Symbol.h"

namespace clang {
class NamedDecl;
} // namespace clang

namespace facts {

template <>
ExtractionResult<Symbol> extractSymbol<Symbol, clang::NamedDecl>(
    const clang::NamedDecl &node, const clang::SourceManager &sourceManager);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_NAMEDDECL_H
