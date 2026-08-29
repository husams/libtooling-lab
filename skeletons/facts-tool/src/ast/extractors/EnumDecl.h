#ifndef FACTS_TOOL_AST_EXTRACTORS_ENUMDECL_H
#define FACTS_TOOL_AST_EXTRACTORS_ENUMDECL_H

#include "ast/Indexing.h"
#include "ast/extractors/Extraction.h"
#include "ast/extractors/Type.h"
#include "model/Enumeration.h"

namespace clang {
class ASTContext;
class EnumDecl;
class SourceManager;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

DetailedExtractionResult<Enumeration>
extractEnumeration(const clang::EnumDecl &node,
                   const clang::SourceManager &sourceManager,
                   FileManager &files, FactStore &store);

IndexingResult collectSymbol(clang::EnumDecl &node, clang::ASTContext &context,
                             FileManager &files, FactStore &store);

} // namespace facts

#endif
