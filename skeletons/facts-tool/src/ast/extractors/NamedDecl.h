#ifndef FACTS_TOOL_AST_EXTRACTORS_NAMEDDECL_H
#define FACTS_TOOL_AST_EXTRACTORS_NAMEDDECL_H

#include "ast/extractors/Symbol.h"
#include "model/Symbol.h"
#include "model/TemplateArgument.h"

#include <expected>
#include <string>
#include <system_error>
#include <vector>

namespace clang {
class NamedDecl;
class TypedefNameDecl;
} // namespace clang

namespace facts {
class FactStore;

ExtractionResult<std::string> extractUsr(const clang::NamedDecl &node);

template <>
ExtractionResult<Symbol> extractSymbol<Symbol, clang::NamedDecl>(
    const clang::NamedDecl &node, const clang::SourceManager &sourceManager);

std::expected<SymbolId, std::error_code>
extractAliasTarget(const clang::TypedefNameDecl &node, FactStore &store);

ExtractionResult<std::vector<TemplateArgument>>
extractAliasTemplateArguments(const clang::TypedefNameDecl &node,
                              FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_NAMEDDECL_H
