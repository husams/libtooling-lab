#ifndef FACTS_TOOL_AST_EXTRACTORS_SYMBOL_H
#define FACTS_TOOL_AST_EXTRACTORS_SYMBOL_H

#include "ast/extractors/Extraction.h"

namespace clang {
class SourceManager;
} // namespace clang

namespace facts {

template <typename SymbolT, typename NodeT>
ExtractionResult<SymbolT>
extractSymbol(const NodeT &node, const clang::SourceManager &sourceManager);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_SYMBOL_H
