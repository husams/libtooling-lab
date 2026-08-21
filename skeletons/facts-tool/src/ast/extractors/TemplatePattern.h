#ifndef FACTS_TOOL_AST_EXTRACTORS_TEMPLATEPATTERN_H
#define FACTS_TOOL_AST_EXTRACTORS_TEMPLATEPATTERN_H

#include "ast/extractors/Extraction.h"
#include "model/TemplateArgument.h"

#include <vector>

namespace clang {
class TemplateParameterList;
} // namespace clang

namespace facts {
class FactStore;

ExtractionResult<std::vector<TemplateArgument>>
extractTemplateArguments(const clang::TemplateParameterList &parameters,
                         FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_TEMPLATEPATTERN_H
