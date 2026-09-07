#include "storage/ProjectSchema.h"

#include "storage/ProjectSchemaInternal.h"

namespace facts {
namespace {

project_schema::Result addVersionColumn(sqlite3 *database) {
  return project_schema::hasVersionColumn(database).and_then(
      [&](bool present) -> project_schema::Result {
        if (present)
          return {};
        return storage::execute(database,
                                "ALTER TABLE project_registry ADD COLUMN "
                                "schema_version INTEGER NOT NULL DEFAULT 0")
            .transform_error([](auto error) { return error.message(); });
      });
}

project_schema::Result createIndex(sqlite3 *database) {
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
  return storage::execute(database, sql).transform_error([](auto error) {
    return error.message();
  });
}

} // namespace

project_schema::Result migrateProjectSchema(sqlite3 *database) {
  return storage::Transaction::immediate(database)
      .transform_error([](auto error) { return error.message(); })
      .and_then([&](storage::Transaction transaction) {
        return addVersionColumn(database)
            .and_then([&] { return project_schema::version(database); })
            .and_then(project_schema::rejectNewer)
            .and_then([&] { return project_schema::version(database); })
            .and_then([&](int value) {
              return value == 0 ? createIndex(database)
                                : project_schema::Result{};
            })
            .and_then([&] {
              return transaction.commit().transform_error(
                  [](auto error) { return error.message(); });
            });
      });
}

} // namespace facts
