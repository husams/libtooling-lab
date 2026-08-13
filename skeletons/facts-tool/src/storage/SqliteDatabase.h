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
// Because the streams are lazy, the Database must outlive any range taken
// from it.

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

struct ConnectionDeleter {
  void operator()(sqlite3 *database) const noexcept { sqlite3_close(database); }
};

class Database {
public:
  static constexpr int readOnly = SQLITE_OPEN_READONLY;
  static constexpr int readWrite = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;

  static std::expected<Database, std::error_code>
  open(const std::string &path, int flags = readOnly) {
    sqlite3 *raw = nullptr;
    const int status = sqlite3_open_v2(path.c_str(), &raw, flags, nullptr);
    Database database(raw);
    if (status != SQLITE_OK) {
      return std::unexpected(
          raw ? sqliteError(raw)
              : std::error_code(status, std::generic_category()));
    }
    sqlite3_busy_timeout(raw, 10000);
    return database;
  }

  // The raw row stream: cheapest, but a Row only lives until the next step.
  template <detail::Bindable... Binds>
  Generator<Row> rows(std::string sql, Binds... binds) const {
    return detail::rowStream(handle(), std::move(sql), std::move(binds)...);
  }

  // The composable one: each row is mapped to an owned value while it is still
  // valid, so the result pipes into std::ranges views like any other range.
  template <typename Map, detail::Bindable... Binds>
  auto query(std::string sql, Map map, Binds... binds) const
      -> Generator<std::invoke_result_t<Map &, const Row &>> {
    return detail::valueStream(handle(), std::move(sql), std::move(map),
                               std::move(binds)...);
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

  explicit operator bool() const noexcept { return connection_ != nullptr; }

private:
  explicit Database(sqlite3 *connection) : connection_(connection) {}

  sqlite3 *handle() const noexcept { return connection_.get(); }

  std::unique_ptr<sqlite3, ConnectionDeleter> connection_;
};

} // namespace facts::storage

#endif // FACTS_TOOL_STORAGE_SQLITE_DATABASE_H
