#ifndef FACTS_TOOL_STORAGE_SQLITE_CONNECTION_H
#define FACTS_TOOL_STORAGE_SQLITE_CONNECTION_H

// A header-only, move-only sqlite3* owner. FileDatabase keeps its own private
// connection behind a pimpl; this one exists so the coroutine query API in
// SqliteQuery.h can be used against any database file on its own.

#include <sqlite3.h>

#include <expected>
#include <memory>
#include <string>
#include <system_error>

namespace facts::sql {

// SQLite result codes are small positive ints, so they ride in an error_code
// the same way FileDatabase reports them.
inline std::error_code lastError(sqlite3 *database) {
  return {sqlite3_extended_errcode(database), std::generic_category()};
}

struct ConnectionDeleter {
  void operator()(sqlite3 *database) const noexcept { sqlite3_close(database); }
};

class Connection {
public:
  static constexpr int readOnly = SQLITE_OPEN_READONLY;
  static constexpr int readWrite = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;

  static std::expected<Connection, std::error_code>
  open(const std::string &path, int flags = readOnly) {
    sqlite3 *raw = nullptr;
    const int status = sqlite3_open_v2(path.c_str(), &raw, flags, nullptr);
    Connection connection(raw);
    if (status != SQLITE_OK) {
      return std::unexpected(raw ? lastError(raw)
                                 : std::error_code(status,
                                                   std::generic_category()));
    }
    sqlite3_busy_timeout(raw, 10000);
    return connection;
  }

  sqlite3 *handle() const noexcept { return database_.get(); }
  explicit operator bool() const noexcept { return database_ != nullptr; }

  std::expected<void, std::error_code> execute(const char *sql) {
    if (sqlite3_exec(handle(), sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
      return std::unexpected(lastError(handle()));
    }
    return {};
  }

private:
  explicit Connection(sqlite3 *database) : database_(database) {}

  std::unique_ptr<sqlite3, ConnectionDeleter> database_;
};

} // namespace facts::sql

#endif // FACTS_TOOL_STORAGE_SQLITE_CONNECTION_H
