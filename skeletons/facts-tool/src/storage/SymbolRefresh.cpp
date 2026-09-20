#include "storage/SymbolRefresh.h"
#include "storage/SqliteDatabase.h"
namespace facts::storage {
std::expected<void, std::error_code>
beginSymbolRefresh(Database &database, std::span<const FileId> selected) {
  if (sqlite3_get_autocommit(database.nativeHandle()))
    return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
  return database.executeScript(R"sql(
CREATE TEMP TABLE refresh_files(id INTEGER PRIMARY KEY);
CREATE TEMP TABLE refresh_old(id INTEGER PRIMARY KEY);
CREATE TEMP TABLE refresh_seen(id INTEGER PRIMARY KEY);
)sql").and_then([&] {
    return database.executeBulk("INSERT INTO refresh_files VALUES(?1)", selected,
        [](sqlite3_stmt *statement, FileId file) { return bindInteger(statement, 1, file); });
  }).and_then([&](auto) {
    return database.executeScript(R"sql(
INSERT INTO refresh_old
 SELECT s.id FROM symbol s JOIN refresh_files f
 ON s.id>=(f.id<<32) AND s.id<=((f.id<<32)|4294967295)
 WHERE s.is_external=0;
-- Local pointer values are historical identities, not the declaration index.
-- Their Clang USRs contain offsets and survive removal of pointer-call evidence.
CREATE TEMP TABLE refresh_preserved AS SELECT s.id FROM symbol s
 JOIN refresh_old old ON old.id=s.id WHERE s.node=4 AND (s.properties&128)!=0;
DELETE FROM refresh_old WHERE id IN (SELECT id FROM refresh_preserved);
UPDATE symbol SET is_definition=0 WHERE id IN (SELECT id FROM refresh_old)
 OR (id IN (SELECT symbol_id FROM definition WHERE file_id IN (SELECT id FROM refresh_files))
     AND id NOT IN (SELECT id FROM refresh_preserved));
DELETE FROM definition WHERE file_id IN (SELECT id FROM refresh_files)
 AND symbol_id NOT IN (SELECT id FROM refresh_preserved);
CREATE TEMP TRIGGER refresh_insert AFTER INSERT ON main.symbol BEGIN
 INSERT INTO refresh_seen SELECT NEW.id
 WHERE NOT EXISTS(SELECT 1 FROM refresh_seen WHERE id=NEW.id);
END;
CREATE TEMP TRIGGER refresh_update AFTER UPDATE ON main.symbol BEGIN
 INSERT INTO refresh_seen SELECT NEW.id
 WHERE NOT EXISTS(SELECT 1 FROM refresh_seen WHERE id=NEW.id);
END;
)sql");
  });
}
std::expected<void, std::error_code> finishSymbolRefresh(Database &database) {
  return database.executeScript(R"sql(
DROP TRIGGER refresh_insert;
DROP TRIGGER refresh_update;
DELETE FROM refresh_old WHERE id IN (SELECT id FROM refresh_seen)
 OR id IN (SELECT symbol_id FROM definition);
UPDATE relation_site SET receiver_type_id=NULL
 WHERE receiver_type_id IN (SELECT id FROM refresh_old);
DELETE FROM symbol WHERE id IN (SELECT id FROM refresh_old);
UPDATE symbol SET is_definition=1
 WHERE id IN (SELECT symbol_id FROM definition);
DROP TABLE refresh_preserved;
DROP TABLE refresh_files;
DROP TABLE refresh_old;
DROP TABLE refresh_seen;
)sql");
}
}
