#include "config/Configuration.h"

#include <sqlite3.h>
#include <filesystem>

namespace facts::config {
std::expected<void, std::string> ensureOwnedDatabase(const Resolved &resolved) {
  std::error_code error;
  const bool existed = std::filesystem::exists(resolved.database, error);
  std::filesystem::create_directories(resolved.database.parent_path(), error);
  if (error) return std::unexpected(error.message());
  sqlite3 *db = nullptr;
  if (sqlite3_open_v2(resolved.database.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
    const auto message = db ? sqlite3_errmsg(db) : "sqlite3_open_v2 failed";
    if (db) sqlite3_close(db);
    return std::unexpected("cannot create project configuration: " + std::string(message));
  }
  sqlite3_busy_timeout(db, 5000);
  auto finish = [&](std::string message = {}) -> std::expected<void, std::string> {
    sqlite3_close(db);
    return message.empty() ? std::expected<void, std::string>{} : std::unexpected(message);
  };
  if (sqlite3_exec(db, "BEGIN IMMEDIATE; CREATE TABLE IF NOT EXISTS generated_conf_owner(project_root TEXT PRIMARY KEY);", nullptr, nullptr, nullptr) != SQLITE_OK)
    return finish(sqlite3_errmsg(db));
  sqlite3_stmt *statement = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT project_root FROM generated_conf_owner", -1,
                         &statement, nullptr) != SQLITE_OK)
    return finish(sqlite3_errmsg(db));
  const auto status = sqlite3_step(statement);
  if (status != SQLITE_ROW && status != SQLITE_DONE) {
    const auto message = std::string(sqlite3_errmsg(db));
    sqlite3_finalize(statement);
    sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
    return finish(message);
  }
  const auto existing = status == SQLITE_ROW
                            ? reinterpret_cast<const char *>(
                                  sqlite3_column_text(statement, 0))
                            : nullptr;
  sqlite3_finalize(statement);
  const auto collision = "generated conf path collision: " + resolved.database.string() +
                         "; use --conf to select an existing database explicitly";
  if (existing && resolved.projectRoot.string() != existing) { sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr); return finish(collision); }
  if (existed && !existing) { sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr); return finish(collision); }
  if (existing == nullptr) {
    if (sqlite3_prepare_v2(db, "INSERT INTO generated_conf_owner VALUES(?)", -1,
                           &statement, nullptr) != SQLITE_OK) {
      const auto message = std::string(sqlite3_errmsg(db));
      sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
      return finish(message);
    }
    const bool bound = sqlite3_bind_text(statement, 1,
                                         resolved.projectRoot.c_str(), -1,
                                         SQLITE_TRANSIENT) == SQLITE_OK;
    const auto inserted = bound ? sqlite3_step(statement) : SQLITE_ERROR;
    sqlite3_finalize(statement);
    if (inserted != SQLITE_DONE) {
      const auto message = std::string(sqlite3_errmsg(db));
      sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
      return finish(message);
    }
  }
  if (sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr) != SQLITE_OK)
    return finish(sqlite3_errmsg(db));
  return finish();
}
} // namespace facts::config
