#pragma once
#include "storage/catalog/Records.h"

namespace facts::commands {
catalog::Result<std::string>
exportCommands(catalog::Database &database, const std::string &configuration,
               const catalog::Component &component);
} // namespace facts::commands
