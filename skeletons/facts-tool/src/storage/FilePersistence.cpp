#include "storage/FilePersistence.h"

#include "storage/Sqlite.h"

#include <sqlite3.h>

#include <limits>

namespace facts {
namespace {

std::expected<void, std::error_code> bindText(sqlite3 *database,
                                              sqlite3_stmt *statement,
                                              int position,
                                              std::string_view value) {
  return storage::bindText(statement, position, value)
             ? std::expected<void, std::error_code>{}
             : std::expected<void, std::error_code>{
                   std::unexpected(storage::sqliteError(database))};
}

std::expected<std::int64_t, std::error_code>
readRowId(sqlite3 *database, sqlite3_stmt *statement) {
  const auto status = sqlite3_step(statement);
  if (status == SQLITE_DONE) {
    return std::unexpected(
        std::make_error_code(std::errc::no_such_file_or_directory));
  }
  if (status != SQLITE_ROW) {
    return std::unexpected(storage::sqliteError(database));
  }
  const auto raw = sqlite3_column_int64(statement, 0);
  if (raw < 1) {
    return std::unexpected(std::make_error_code(std::errc::value_too_large));
  }
  return raw;
}

std::expected<FileId, std::error_code> decodeFileId(std::int64_t raw) {
  if (raw < firstPhysicalFileId || raw > std::numeric_limits<FileId>::max()) {
    return std::unexpected(std::make_error_code(std::errc::value_too_large));
  }
  return static_cast<FileId>(raw);
}

std::expected<void, std::error_code>
bindIdentity(sqlite3 *database, sqlite3_stmt *statement,
             const FileIdentity &identity) {
  if (!storage::bindInteger(statement, 1, identity.componentId)) {
    return std::unexpected(storage::sqliteError(database));
  }
  return bindText(database, statement, 2, identity.directory).and_then([&] {
    return bindText(database, statement, 3, identity.name);
  });
}

} // namespace

std::expected<std::int64_t, std::error_code> fileRowCount(sqlite3 *database) {
  return storage::prepare(database, "SELECT COUNT(*) FROM file")
      .and_then([&](storage::Statement statement)
                    -> std::expected<std::int64_t, std::error_code> {
        if (sqlite3_step(statement.get()) != SQLITE_ROW) {
          return std::unexpected(storage::sqliteError(database));
        }
        const auto count = sqlite3_column_int64(statement.get(), 0);
        return count >= 0 ? std::expected<std::int64_t, std::error_code>{count}
                          : std::expected<std::int64_t, std::error_code>{
                                std::unexpected(std::make_error_code(
                                    std::errc::value_too_large))};
      });
}

std::expected<std::int64_t, std::error_code>
upsertDirectory(sqlite3 *database, std::int64_t componentId,
                std::string_view directory) {
  constexpr std::string_view sql =
      "INSERT INTO directory(component_id,path) VALUES(?1,?2) "
      "ON CONFLICT(component_id,path) DO UPDATE SET path=excluded.path "
      "RETURNING id";
  return storage::prepare(database, sql)
      .and_then([&](storage::Statement statement) {
        if (!storage::bindInteger(statement.get(), 1, componentId)) {
          return std::expected<std::int64_t, std::error_code>{
              std::unexpected(storage::sqliteError(database))};
        }
        return bindText(database, statement.get(), 2, directory).and_then([&] {
          return readRowId(database, statement.get());
        });
      });
}

std::expected<FileId, std::error_code>
persistFile(sqlite3 *database, const FileIdentity &identity) {
  return upsertDirectory(database, identity.componentId, identity.directory)
      .and_then([&](std::int64_t directoryId) {
        constexpr std::string_view sql =
            "INSERT INTO file(directory_id,name) VALUES(?1,?2) "
            "ON CONFLICT(directory_id,name) DO UPDATE SET name=excluded.name "
            "RETURNING id";
        return storage::prepare(database, sql)
            .and_then([&](storage::Statement statement)
                          -> std::expected<FileId, std::error_code> {
              if (!storage::bindInteger(statement.get(), 1, directoryId)) {
                return std::unexpected(storage::sqliteError(database));
              }
              return bindText(database, statement.get(), 2, identity.name)
                  .and_then(
                      [&] { return readRowId(database, statement.get()); })
                  .and_then(decodeFileId);
            });
      });
}

std::expected<void, std::error_code>
persistFiles(sqlite3 *database, std::span<const FileIdentity> identities) {
  for (const auto &identity : identities) {
    if (auto persisted = persistFile(database, identity); !persisted) {
      return std::unexpected(persisted.error());
    }
  }
  return {};
}

std::expected<FileId, std::error_code>
selectFileId(sqlite3 *database, const FileIdentity &identity) {
  constexpr std::string_view sql =
      "SELECT file.id FROM file "
      "JOIN directory ON directory.id=file.directory_id "
      "WHERE directory.component_id=?1 "
      "AND directory.path=?2 AND file.name=?3";
  return storage::prepare(database, sql)
      .and_then([&](storage::Statement statement) {
        return bindIdentity(database, statement.get(), identity)
            .and_then([&] { return readRowId(database, statement.get()); })
            .and_then(decodeFileId);
      });
}

std::expected<void, std::error_code>
insertFileWithId(sqlite3 *database, FileId id, const FileIdentity &identity) {
  return upsertDirectory(database, identity.componentId, identity.directory)
      .and_then([&](std::int64_t directoryId) {
        constexpr std::string_view sql =
            "INSERT INTO file(id,directory_id,name) VALUES(?1,?2,?3)";
        return storage::prepare(database, sql)
            .and_then([&](storage::Statement statement)
                          -> std::expected<void, std::error_code> {
              if (!storage::bindInteger(statement.get(), 1, id) ||
                  !storage::bindInteger(statement.get(), 2, directoryId)) {
                return std::unexpected(storage::sqliteError(database));
              }
              return bindText(database, statement.get(), 3, identity.name)
                  .and_then([&]() -> std::expected<void, std::error_code> {
                    return sqlite3_step(statement.get()) == SQLITE_DONE
                               ? std::expected<void, std::error_code>{}
                               : std::expected<void, std::error_code>{
                                     std::unexpected(
                                         storage::sqliteError(database))};
                  });
            });
      });
}

} // namespace facts
