#pragma once

#include <expected>
#include <string>

struct sqlite3;

namespace facts {

inline constexpr int currentProjectSchemaVersion = 1;

std::expected<void, std::string> migrateProjectSchema(sqlite3 *database);
std::expected<void, std::string>
requireSupportedProjectSchema(sqlite3 *database);
std::expected<void, std::string> requireCurrentProjectSchema(sqlite3 *database);

} // namespace facts
