#ifndef FACTS_TOOL_AST_EXTRACTORS_ENUMDECL_H
#define FACTS_TOOL_AST_EXTRACTORS_ENUMDECL_H

#include "ast/extractors/Extraction.h"
#include "model/Enumeration.h"

namespace clang {
class ASTContext;
class EnumDecl;
class SourceManager;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

ExtractionResult<Enumeration>
extractEnumeration(const clang::EnumDecl &node,
                   const clang::SourceManager &sourceManager, FactStore &store);

void collectSymbol(clang::EnumDecl &node, clang::ASTContext &context,
                   FileManager &files, FactStore &store);

} // namespace facts

#endif
