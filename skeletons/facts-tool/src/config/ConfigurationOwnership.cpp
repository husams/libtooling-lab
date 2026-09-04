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
  sqlite3_prepare_v2(db, "SELECT project_root FROM generated_conf_owner", -1, &statement, nullptr);
  const auto existing = sqlite3_step(statement) == SQLITE_ROW ? reinterpret_cast<const char *>(sqlite3_column_text(statement, 0)) : nullptr;
  sqlite3_finalize(statement);
  const auto collision = "generated conf path collision: " + resolved.database.string() +
                         "; use --conf to select an existing database explicitly";
  if (existing && resolved.projectRoot.string() != existing) { sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr); return finish(collision); }
  if (existed && !existing) { sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr); return finish(collision); }
  if (existing == nullptr) {
    sqlite3_prepare_v2(db, "INSERT INTO generated_conf_owner VALUES(?)", -1, &statement, nullptr);
    sqlite3_bind_text(statement, 1, resolved.projectRoot.c_str(), -1, SQLITE_TRANSIENT); sqlite3_step(statement); sqlite3_finalize(statement);
  }
  sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
  return finish();
}
} // namespace facts::config
