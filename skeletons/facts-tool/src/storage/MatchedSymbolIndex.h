#pragma once

#include "model/MatchedSymbol.h"

#include <expected>
#include <span>
#include <system_error>

namespace facts::storage {
class Database;

std::expected<void, std::error_code>
upsertMatchedSymbols(Database &database,
                     std::span<const MatchedSymbol> symbols);

} // namespace facts::storage
