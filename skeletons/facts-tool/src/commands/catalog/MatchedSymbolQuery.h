#pragma once

#include "commands/catalog/MatchedSymbolData.h"
#include "storage/catalog/Database.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace facts::commands {

catalog::Result<std::vector<MatchedSymbolCandidate>>
findMatchedSymbols(catalog::Database &database,
                   const std::optional<std::string> &usr,
                   const std::optional<std::string> &name,
                   const std::optional<std::int64_t> &kind);
catalog::Result<std::size_t> clearMatchedSymbols(catalog::Database &database,
                                                 FileId fileId);

} // namespace facts::commands
