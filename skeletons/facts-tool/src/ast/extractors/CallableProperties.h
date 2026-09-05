#ifndef FACTS_TOOL_AST_EXTRACTORS_CALLABLEPROPERTIES_H
#define FACTS_TOOL_AST_EXTRACTORS_CALLABLEPROPERTIES_H
#include "ast/extractors/Extraction.h"
#include "model/Function.h"

namespace clang {
class FunctionDecl;
}

namespace facts {
ExtractionResult<Function>
addCallableProperties(Function function, const clang::FunctionDecl &node);
}
#endif
