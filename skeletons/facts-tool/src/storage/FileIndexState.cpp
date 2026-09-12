#include "storage/FileIndexState.h"

#include "storage/Sqlite.h"
#include "storage/SqliteDatabase.h"

#include <sqlite3.h>

#include <string_view>

namespace facts {
namespace {

std::expected<bool, std::error_code> hasFileColumn(sqlite3 *database,
                                                   std::string_view name) {
  constexpr std::string_view sql =
      "SELECT EXISTS(SELECT 1 FROM pragma_table_info('file') WHERE name=?1)";
  return storage::prepare(database, sql)
      .and_then([&](storage::Statement statement)
                    -> std::expected<bool, std::error_code> {
        if (!storage::bindText(statement.get(), 1, name)) {
          return std::unexpected(storage::sqliteError(database));
        }
        if (sqlite3_step(statement.get()) != SQLITE_ROW) {
          return std::unexpected(storage::sqliteError(database));
        }
        return sqlite3_column_int(statement.get(), 0) != 0;
      });
}

} // namespace

std::expected<bool, std::error_code>
fileIndexStateColumnsPresent(sqlite3 *database) {
  return hasFileColumn(database, "facts_db").and_then([&](bool factsDb) {
    return hasFileColumn(database, "git_commit").transform([&](bool gitCommit) {
      return factsDb && gitCommit;
    });
  });
}

std::expected<FileIndexState, std::error_code>
readFileIndexStateRow(sqlite3 *database, FileId id) {
  constexpr std::string_view sql =
      "SELECT indexed,coalesce(indexed_at,''),mtime,"
      "coalesce(facts_db,''),git_commit FROM file WHERE id=?1";
  return storage::prepare(database, sql)
      .and_then([&](storage::Statement statement)
                    -> std::expected<FileIndexState, std::error_code> {
        if (!storage::bindInteger(statement.get(), 1, id)) {
          return std::unexpected(storage::sqliteError(database));
        }
        const auto status = sqlite3_step(statement.get());
        if (status == SQLITE_DONE) {
          return FileIndexState{};
        }
        if (status != SQLITE_ROW) {
          return std::unexpected(storage::sqliteError(database));
        }
        FileIndexState state;
        state.indexed = sqlite3_column_int(statement.get(), 0) != 0;
        state.indexedAt = storage::columnText(statement.get(), 1);
        if (sqlite3_column_type(statement.get(), 2) != SQLITE_NULL) {
          state.mtime = sqlite3_column_double(statement.get(), 2);
        }
        state.factsDb = storage::columnText(statement.get(), 3);
        if (sqlite3_column_type(statement.get(), 4) != SQLITE_NULL) {
          state.gitCommit = storage::columnText(statement.get(), 4);
        }
        return state;
      });
}

std::expected<FileIndexState, std::error_code>
readFileIndexState(sqlite3 *database, FileId id) {
  return fileIndexStateColumnsPresent(database).and_then(
      [&](bool present) -> std::expected<FileIndexState, std::error_code> {
        if (!present) {
          return FileIndexState{};
        }
        return readFileIndexStateRow(database, id);
      });
}

std::expected<void, std::error_code>
markFilesIndexed(storage::Database &database,
                 std::span<const FileIndexRecord> records) {
  return database
      .executeBulk(
          "UPDATE file SET indexed=1,indexed_at=?1,mtime=?2,facts_db=?3,"
          "git_commit=?4 WHERE id=?5",
          records,
          [](sqlite3_stmt *statement, const FileIndexRecord &record) {
            return storage::bindParameters(statement, record.indexedAt,
                                           record.mtime, record.factsDb,
                                           record.gitCommit, record.id);
          })
      .transform([](const storage::BulkResult &) {});
}

} // namespace facts
