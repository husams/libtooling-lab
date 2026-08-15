#ifndef FACTS_TOOL_AST_EXTRACTORS_VARDECL_H
#define FACTS_TOOL_AST_EXTRACTORS_VARDECL_H

#include "ast/extractors/Extraction.h"
#include "model/Variable.h"

namespace clang {
class ASTContext;
class SourceManager;
class VarDecl;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

ExtractionResult<Variable>
extractVariable(const clang::VarDecl &node,
                const clang::SourceManager &sourceManager);

void collectSymbol(clang::VarDecl &node, clang::ASTContext &context,
                   FileManager &files, FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_VARDECL_H
