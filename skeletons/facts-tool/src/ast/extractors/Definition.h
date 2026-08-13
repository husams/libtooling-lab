#ifndef FACTS_TOOL_AST_EXTRACTORS_DEFINITION_H
#define FACTS_TOOL_AST_EXTRACTORS_DEFINITION_H

#include "ast/extractors/Extraction.h"
#include "model/Function.h"
#include "model/Record.h"
#include "model/Symbol.h"

namespace clang {
class Decl;
} // namespace clang

namespace facts {

template <typename Model>
ExtractionResult<Model>
addDefinitionRegion(Model model, const clang::Decl &node, bool isDefinition,
                    const clang::SourceManager &sourceManager);

extern template ExtractionResult<Function>
addDefinitionRegion(Function model, const clang::Decl &node, bool isDefinition,
                    const clang::SourceManager &sourceManager);

extern template ExtractionResult<Record>
addDefinitionRegion(Record model, const clang::Decl &node, bool isDefinition,
                    const clang::SourceManager &sourceManager);

} // namespace facts

#endif
