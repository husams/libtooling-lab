#include "apis/index/Internal.h"
#include "apis/index/Tables.h"

namespace facts::apis::index {
Result<void> initialize(Database &database) {
  constexpr auto sql = R"sql(
CREATE TABLE IF NOT EXISTS global_symbol_index_state (
  id INTEGER PRIMARY KEY CHECK(id=1),
  generation INTEGER NOT NULL DEFAULT 0,
  sources INTEGER NOT NULL DEFAULT 0,
  missing_sources INTEGER NOT NULL DEFAULT 0
);
INSERT OR IGNORE INTO global_symbol_index_state(id) VALUES(1);
CREATE TABLE IF NOT EXISTS api_index_invalidated_file (file_id INTEGER PRIMARY KEY);
DELETE FROM api_index_invalidated_file WHERE file_id IN (SELECT id FROM file WHERE indexed=1)
 OR file_id NOT IN (SELECT id FROM file);
)sql";
  return storage::execute(database.nativeHandle(), symbolTableSql)
      .and_then([&] { return storage::execute(database.nativeHandle(), sql); })
      .transform_error([&](auto) { return catalog::databaseError(database); })
      .and_then([&] { return ensurePositions(database); })
      .and_then([&] { return catalog::execute(database, symbolLookupSql); })
      .and_then([&] { return ensureStateCounts(database); })
      .and_then([&] { return ensureIdentities(database); });
}
Result<void> prepareStage(Database &database) {
  constexpr auto sql = R"sql(
CREATE TEMP TABLE api_symbol_stage (
  usr TEXT NOT NULL,
  qualified_name TEXT NOT NULL,
  file_id INTEGER NOT NULL,
  kind TEXT NOT NULL,
  path TEXT NOT NULL,
  is_definition INTEGER NOT NULL,
  PRIMARY KEY(usr,file_id)
) WITHOUT ROWID;
CREATE TEMP TABLE api_symbol_owner (file_id INTEGER PRIMARY KEY,facts_db TEXT NOT NULL);
)sql";
  return storage::execute(database.nativeHandle(), sql)
      .transform_error([&](auto) { return catalog::databaseError(database); });
}
}
