#ifndef FACTS_TOOL_AST_EXTRACTORS_TEMPLATEPATTERN_H
#define FACTS_TOOL_AST_EXTRACTORS_TEMPLATEPATTERN_H

#include "ast/extractors/Extraction.h"
#include "ast/extractors/Type.h"
#include "model/TemplateArgument.h"

#include <vector>

namespace clang {
class SourceManager;
class TemplateParameterList;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

DetailedExtractionResult<std::vector<TemplateArgument>>
extractTemplateArguments(const clang::TemplateParameterList &parameters,
                         const clang::SourceManager &sourceManager,
                         FileManager &files, FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_TEMPLATEPATTERN_H
