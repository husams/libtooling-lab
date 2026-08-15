#ifndef FACTS_TOOL_AST_EXTRACTORS_ENUMCONSTANTDECL_H
#define FACTS_TOOL_AST_EXTRACTORS_ENUMCONSTANTDECL_H

#include "ast/extractors/Extraction.h"
#include "model/Enumerator.h"

namespace clang {
class ASTContext;
class EnumConstantDecl;
class SourceManager;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

ExtractionResult<Enumerator>
extractEnumerator(const clang::EnumConstantDecl &node,
                  const clang::SourceManager &sourceManager);

void collectSymbol(clang::EnumConstantDecl &node, clang::ASTContext &context,
                   FileManager &files, FactStore &store);

} // namespace facts

#endif
