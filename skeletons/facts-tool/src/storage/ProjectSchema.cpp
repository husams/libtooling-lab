#include "storage/ProjectSchema.h"
#include "storage/Sqlite.h"
#include <sqlite3.h>
namespace facts {
namespace {
using Result = std::expected<void, std::string>;
std::expected<int, std::string> version(sqlite3 *database) {
  auto statement = storage::prepare(
      database, "SELECT schema_version FROM project_registry WHERE id=1");
  if (!statement || sqlite3_step(statement->get()) != SQLITE_ROW)
    return std::unexpected("cannot read project schema version");
  return sqlite3_column_int(statement->get(), 0);
}
std::expected<bool, std::string> hasVersionColumn(sqlite3 *database) {
  constexpr auto sql =
      "SELECT count(*) FROM pragma_table_info('project_registry') "
      "WHERE name='schema_version'";
  auto statement = storage::prepare(database, sql);
  if (!statement || sqlite3_step(statement->get()) != SQLITE_ROW)
    return std::unexpected("cannot inspect project schema version");
  return sqlite3_column_int(statement->get(), 0) != 0;
}
Result addVersionColumn(sqlite3 *database) {
  return hasVersionColumn(database).and_then([&](bool present) -> Result {
    if (present)
      return {};
    return storage::execute(database,
                            "ALTER TABLE project_registry ADD COLUMN "
                            "schema_version INTEGER NOT NULL DEFAULT 0")
        .transform_error([&](auto error) { return error.message(); });
  });
}
Result createIndex(sqlite3 *database) {
  constexpr auto sql = R"sql(
CREATE TABLE matched_symbol_index (
  usr TEXT NOT NULL CHECK(usr <> ''),
  qualified_name TEXT NOT NULL,
  file_id INTEGER NOT NULL REFERENCES file(id) ON DELETE CASCADE,
  kind INTEGER NOT NULL,
  PRIMARY KEY(usr, file_id)
) WITHOUT ROWID;
CREATE INDEX matched_symbol_name ON matched_symbol_index(qualified_name, kind);
UPDATE project_registry SET schema_version=1 WHERE id=1;
)sql";
  return storage::execute(database, sql).transform_error([&](auto error) {
    return error.message();
  });
}
Result rejectNewer(int value) {
  return value > currentProjectSchemaVersion
             ? Result{std::unexpected("unsupported-project-schema: " +
                                      std::to_string(value))}
             : Result{};
}
} // namespace
Result migrateProjectSchema(sqlite3 *database) {
  return storage::Transaction::immediate(database)
      .transform_error([](auto error) { return error.message(); })
      .and_then([&](storage::Transaction transaction) {
        return addVersionColumn(database)
            .and_then([&] { return version(database); })
            .and_then(rejectNewer)
            .and_then([&] { return version(database); })
            .and_then([&](int value) {
              return value == 0 ? createIndex(database) : Result{};
            })
            .and_then([&] {
              return transaction.commit().transform_error(
                  [](auto error) { return error.message(); });
            });
      });
}
Result requireSupportedProjectSchema(sqlite3 *database) {
  return hasVersionColumn(database).and_then([&](bool present) -> Result {
    return present ? version(database).and_then(rejectNewer) : Result{};
  });
}
Result requireCurrentProjectSchema(sqlite3 *database) {
  return hasVersionColumn(database).and_then([&](bool present) -> Result {
    if (!present)
      return std::unexpected(
          "project configuration uses an outdated project "
          "schema; re-run 'facts-tool import' to migrate it");
    return version(database).and_then([](int value) -> Result {
      return value == currentProjectSchemaVersion
                 ? Result{}
                 : Result{std::unexpected(
                       value > currentProjectSchemaVersion
                           ? "unsupported-project-schema: " +
                                 std::to_string(value)
                           : "project configuration uses an outdated project "
                             "schema; re-run 'facts-tool import' to migrate "
                             "it")};
    });
  });
}
} // namespace facts
