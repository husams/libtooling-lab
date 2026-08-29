#ifndef FACTS_TOOL_AST_EXTRACTORS_FUNCTIONTEMPLATE_H
#define FACTS_TOOL_AST_EXTRACTORS_FUNCTIONTEMPLATE_H

#include "ast/Indexing.h"
#include "ast/extractors/Extraction.h"
#include "ast/extractors/Type.h"
#include "model/Function.h"
#include "model/FunctionInstance.h"
#include "model/FunctionTemplate.h"

#include <expected>
#include <system_error>

namespace clang {
class FunctionDecl;
class SourceManager;
class TemplateParameterList;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

DetailedExtractionResult<FunctionTemplate>
toFunctionTemplate(Function function,
                   const clang::TemplateParameterList &parameters,
                   const clang::SourceManager &sourceManager,
                   FileManager &files, FactStore &store);

DetailedExtractionResult<FunctionInstance>
toFunctionInstance(Function function, const clang::FunctionDecl &node,
                   FileManager &files, FactStore &store);

IndexingResult storeFunctionInstanceRelations(const clang::FunctionDecl &node,
                                              SymbolId instance,
                                              FileManager &files,
                                              FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_FUNCTIONTEMPLATE_H
