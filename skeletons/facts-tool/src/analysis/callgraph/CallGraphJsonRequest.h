#pragma once

#include "analysis/callgraph/CallGraphTraversal.h"

#include <llvm/Support/JSON.h>

namespace facts::callgraph {

llvm::json::Object queryJson(const RenderedGraph &traversal);
llvm::json::Object truncationJson(const QueryGraph &graph,
                                  const RenderedGraph &traversal);
llvm::json::Object excludedScopeJson(const QueryGraph &graph,
                                     const RenderedGraph &traversal);

} // namespace facts::callgraph
