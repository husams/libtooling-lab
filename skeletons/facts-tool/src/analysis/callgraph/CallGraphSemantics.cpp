#include "analysis/callgraph/CallGraphSemantics.h"

#include "storage/SemanticProperties.h"

#include <clang/Index/IndexSymbol.h>

namespace facts::callgraph {

std::string_view edgeViewName(EdgeView view) {
  return view == EdgeView::Semantic ? "semantic" : "calls";
}

std::string_view semanticKind(const QueryNode *target, RelationKind relation) {
  if (relation == RelationKind::DispatchCalls)
    return "virtual_dispatch";
  if (!target)
    return "function";
  const auto kind = storage::symbolKindFromStored(target->kind);
  if (kind == clang::index::SymbolKind::Constructor)
    return "constructor";
  if (kind == clang::index::SymbolKind::Destructor)
    return "destructor";
  if (target->name.find("::<lambda@") != std::string::npos)
    return "lambda";
  return "function";
}

} // namespace facts::callgraph
