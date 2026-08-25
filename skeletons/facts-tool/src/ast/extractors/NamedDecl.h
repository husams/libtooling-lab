#ifndef FACTS_TOOL_AST_EXTRACTORS_NAMEDDECL_H
#define FACTS_TOOL_AST_EXTRACTORS_NAMEDDECL_H

#include "ast/extractors/Symbol.h"
#include "ast/extractors/Type.h"
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
class FileManager;

ExtractionResult<std::string> extractUsr(const clang::NamedDecl &node);

// The qualified name as Clang spells it, except that lambda scopes are named
// after their source coordinates so the result does not change with the Clang
// release or the absolute path of the checkout.
std::string extractQualifiedName(const clang::NamedDecl &node,
                                 const clang::SourceManager &sourceManager);

template <>
ExtractionResult<Symbol> extractSymbol<Symbol, clang::NamedDecl>(
    const clang::NamedDecl &node, const clang::SourceManager &sourceManager);

TypeResult extractAliasTarget(const clang::TypedefNameDecl &node,
                              const clang::SourceManager &sourceManager,
                              FileManager &files, FactStore &store);

ExtractionResult<std::vector<TemplateArgument>>
extractAliasTemplateArguments(const clang::TypedefNameDecl &node,
                              const clang::SourceManager &sourceManager,
                              FileManager &files, FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_NAMEDDECL_H
