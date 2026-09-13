#pragma once

#include "storage/SqliteDatabase.h"

#include <expected>
#include <string>
#include <string_view>

namespace facts::variableflow::storage {

using Database = facts::storage::Database;
using Row = facts::storage::Row;

constexpr int schemaVersion = 1;

std::expected<void, std::string> prepare(Database &database);
std::expected<void, std::string> initialize(Database &database);
std::expected<void, std::string> requireCurrent(Database &database);

} // namespace facts::variableflow::storage
