#ifndef FACTS_TOOL_AST_EXTRACTORS_FUNCTIONTEMPLATE_H
#define FACTS_TOOL_AST_EXTRACTORS_FUNCTIONTEMPLATE_H

#include "ast/extractors/Extraction.h"
#include "model/Function.h"
#include "model/FunctionInstance.h"
#include "model/FunctionTemplate.h"

#include <expected>
#include <system_error>

namespace clang {
class FunctionDecl;
class TemplateParameterList;
} // namespace clang

namespace facts {
class FactStore;

ExtractionResult<FunctionTemplate>
toFunctionTemplate(Function function,
                   const clang::TemplateParameterList &parameters,
                   FactStore &store);

ExtractionResult<FunctionInstance>
toFunctionInstance(Function function, const clang::FunctionDecl &node,
                   FactStore &store);

std::expected<void, std::error_code>
storeFunctionInstanceRelations(const clang::FunctionDecl &node,
                               SymbolId instance, FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_FUNCTIONTEMPLATE_H
