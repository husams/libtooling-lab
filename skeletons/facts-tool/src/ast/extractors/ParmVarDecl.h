#ifndef FACTS_TOOL_AST_EXTRACTORS_PARMVARDECL_H
#define FACTS_TOOL_AST_EXTRACTORS_PARMVARDECL_H

#include "ast/extractors/Extraction.h"
#include "model/Parameter.h"

namespace clang {
class ParmVarDecl;
class SourceManager;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

ExtractionResult<Parameter>
extractParameter(const clang::ParmVarDecl &node,
                 const clang::SourceManager &sourceManager, FileManager &files,
                 FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_PARMVARDECL_H
