#pragma once

#include "analysis/callgraph/CallGraphQuery.h"
#include "storage/catalog/Database.h"

namespace facts::callgraph {
catalog::Result<std::vector<QueryNode>>
loadCallGraphNodes(catalog::Database &database);
}
