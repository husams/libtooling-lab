#ifndef FACTS_TOOL_STORAGE_SQLITE_DATABASE_H
#define FACTS_TOOL_STORAGE_SQLITE_DATABASE_H

// An owning SQLite connection that answers in ranges instead of cursors.
//
//   auto database = storage::Database::open("files.sqlite");
//   for (const auto &row : database->rows("SELECT id, path FROM file")) ...
//
// The sqlite3* stays private, the way Storage and FileDatabase keep theirs:
// prepare, bind, step and finalize live inside the generators in
// SqliteQuery.h, so callers never see a statement handle or a result code.
// Lazy streams share the internal connection state, so they remain valid after
// the Database value is moved or destroyed.

#include "storage/Generator.h"
#include "storage/Sqlite.h"
#include "storage/SqliteQuery.h"

#include <sqlite3.h>

#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

namespace facts::storage {

class Database {
public:
  static constexpr int readOnly = SQLITE_OPEN_READONLY;
  static constexpr int readWrite = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;

  static std::expected<Database, std::error_code> open(const std::string &path,
                                                       int flags = readOnly) {
    sqlite3 *raw = nullptr;
    const int status = sqlite3_open_v2(path.c_str(), &raw, flags, nullptr);
    if (status != SQLITE_OK) {
      const auto error = raw ? sqliteError(raw)
                             : std::error_code(status, std::generic_category());
      if (raw) {
        sqlite3_close_v2(raw);
      }
      return std::unexpected(error);
    }
    sqlite3_busy_timeout(raw, 10000);
    return Database(detail::Connection(
        raw, [](sqlite3 *database) { sqlite3_close_v2(database); }));
  }

  Database(Database &&) noexcept = default;
  Database &operator=(Database &&) noexcept = default;
  Database(const Database &) = delete;
  Database &operator=(const Database &) = delete;

  // The raw row stream: cheapest, but a Row only lives until the next step.
  template <typename... Binds>
  Generator<Row> rows(std::string sql, Binds &&...binds) const {
    return detail::rowStream(connection_, std::move(sql),
                             detail::ownBind(std::forward<Binds>(binds))...);
  }

  // The composable one: each row is mapped to an owned value while it is still
  // valid, so the result pipes into std::ranges views like any other range.
  template <typename Map, typename... Binds>
  auto query(std::string sql, Map map, Binds &&...binds) const
      -> Generator<std::invoke_result_t<Map &, const Row &>> {
    return detail::valueStream(connection_, std::move(sql), std::move(map),
                               detail::ownBind(std::forward<Binds>(binds))...);
  }

  std::expected<void, std::error_code> execute(std::string_view sql) {
    return storage::execute(handle(), sql);
  }

  std::expected<Transaction, std::error_code> read() {
    return Transaction::read(handle());
  }

  std::expected<Transaction, std::error_code> write() {
    return Transaction::write(handle());
  }

  explicit operator bool() const noexcept {
    return connection_ && connection_.get() != nullptr;
  }

private:
  explicit Database(detail::Connection connection)
      : connection_(std::move(connection)) {}

  sqlite3 *handle() const noexcept { return connection_.get(); }

  detail::Connection connection_;
};

} // namespace facts::storage

#endif // FACTS_TOOL_STORAGE_SQLITE_DATABASE_H
