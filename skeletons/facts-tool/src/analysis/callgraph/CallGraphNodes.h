#pragma once

#include "analysis/callgraph/CallGraphQuery.h"
#include "storage/catalog/Database.h"

namespace facts::callgraph {
catalog::Result<std::vector<QueryNode>>
loadCallGraphNodes(catalog::Database &database);
catalog::Result<std::vector<QueryPointerCall>>
loadCallGraphPointerCalls(catalog::Database &database,
                         std::optional<SymbolId> source = std::nullopt);
}
