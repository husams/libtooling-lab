#ifndef FACTS_TOOL_STORAGE_SCHEMAMIGRATION_H
#define FACTS_TOOL_STORAGE_SCHEMAMIGRATION_H

#include <expected>
#include <system_error>

struct sqlite3;

namespace facts::storage {

std::expected<void, std::error_code> migrateSchema(sqlite3 *database);

} // namespace facts::storage

#endif // FACTS_TOOL_STORAGE_SCHEMAMIGRATION_H
