#pragma once

#include "model/SymbolId.h"

#include <sqlite3.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace storage_schema_test {

auto require(bool condition, std::string_view message) -> bool;
auto execute(sqlite3 *database, std::string_view sql) -> bool;
auto scalar(sqlite3 *database, std::string_view sql) -> std::int64_t;
auto textScalar(sqlite3 *database, std::string_view sql) -> std::string;
auto packed(facts::SymbolId id) -> std::uint64_t;
auto noPackedFlags(sqlite3 *database) -> bool;
auto noRedundantSymbolIdColumns(sqlite3 *database) -> bool;
auto usrIsOnlySymbolIdentity(sqlite3 *database) -> bool;
void removeDatabase(const std::filesystem::path &path);

} // namespace storage_schema_test
