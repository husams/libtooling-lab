#pragma once

#include "storage/ProjectSchema.h"
#include "storage/Sqlite.h"

#include <sqlite3.h>

namespace facts::project_schema {

using Result = std::expected<void, std::string>;

inline std::expected<int, std::string> version(sqlite3 *database) {
  auto statement = storage::prepare(
      database, "SELECT schema_version FROM project_registry WHERE id=1");
  if (!statement || sqlite3_step(statement->get()) != SQLITE_ROW)
    return std::unexpected("cannot read project schema version");
  return sqlite3_column_int(statement->get(), 0);
}

inline std::expected<bool, std::string> hasVersionColumn(sqlite3 *database) {
  constexpr auto sql =
      "SELECT count(*) FROM pragma_table_info('project_registry') "
      "WHERE name='schema_version'";
  auto statement = storage::prepare(database, sql);
  if (!statement || sqlite3_step(statement->get()) != SQLITE_ROW)
    return std::unexpected("cannot inspect project schema version");
  return sqlite3_column_int(statement->get(), 0) != 0;
}

inline Result rejectNewer(int value) {
  return value > currentProjectSchemaVersion
             ? Result{std::unexpected("unsupported-project-schema: " +
                                      std::to_string(value))}
             : Result{};
}

} // namespace facts::project_schema
