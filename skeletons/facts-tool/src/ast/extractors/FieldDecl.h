#ifndef FACTS_TOOL_AST_EXTRACTORS_FIELDDECL_H
#define FACTS_TOOL_AST_EXTRACTORS_FIELDDECL_H

#include "ast/Indexing.h"
#include "ast/extractors/Extraction.h"
#include "model/Field.h"

namespace clang {
class ASTContext;
class FieldDecl;
class SourceManager;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

ExtractionResult<Field> extractField(const clang::FieldDecl &node,
                                     const clang::SourceManager &sourceManager);

IndexingResult collectSymbol(clang::FieldDecl &node, clang::ASTContext &context,
                             FileManager &files, FactStore &store);

} // namespace facts

#endif
