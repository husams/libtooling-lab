#pragma once

#include "analysis/callgraph/CallGraphQuery.h"

#include <llvm/Support/JSON.h>

namespace facts::callgraph {
llvm::json::Object pointerCallJson(const QueryPointerCall &call);
} // namespace facts::callgraph
