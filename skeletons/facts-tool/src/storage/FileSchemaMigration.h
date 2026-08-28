#ifndef FACTS_TOOL_STORAGE_FILE_SCHEMA_MIGRATION_H
#define FACTS_TOOL_STORAGE_FILE_SCHEMA_MIGRATION_H

#include <expected>
#include <string>
#include <string_view>
#include <system_error>

struct sqlite3;

namespace facts {

std::expected<void, std::error_code> migrateFileSchema(sqlite3 *database);

std::expected<bool, std::error_code> fileSchemaHasTable(sqlite3 *database,
                                                        std::string_view name);

std::expected<void, std::string> requireCurrentFileSchema(sqlite3 *database);

std::expected<void, std::string>
requireImportedProjectConfiguration(sqlite3 *database);

} // namespace facts

#endif // FACTS_TOOL_STORAGE_FILE_SCHEMA_MIGRATION_H
