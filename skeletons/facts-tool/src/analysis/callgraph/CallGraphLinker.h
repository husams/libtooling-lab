#pragma once

#include "analysis/callgraph/CallGraphTypes.h"
#include "ast/Indexing.h"

namespace facts {
class FactStore;
}

namespace facts::callgraph {

IndexingResult linkCallGraphFacts(CallGraphFacts facts, FactStore &store);

} // namespace facts::callgraph
