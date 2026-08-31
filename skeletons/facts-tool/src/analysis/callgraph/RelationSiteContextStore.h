#pragma once

#include "model/RelationSite.h"

#include <expected>
#include <span>
#include <system_error>

namespace facts::storage {
class Database;
}

namespace facts::callgraph {

bool validContext(const RelationSite &site);
std::expected<void, std::error_code>
insertRelationSites(storage::Database &database,
                    std::span<const RelationSite> sites);

} // namespace facts::callgraph
