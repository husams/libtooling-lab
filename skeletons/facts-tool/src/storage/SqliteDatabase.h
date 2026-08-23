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

#include <concepts>
#include <cstddef>
#include <expected>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

namespace facts::storage {

struct BulkOptions {
  bool atomic{true};
};

struct BulkResult {
  std::size_t processed;
  std::int64_t changes;

  bool operator==(const BulkResult &) const = default;
};

template <typename Binder>
class BulkTerminal;

namespace detail {

inline std::error_code bindingError(sqlite3 *database) {
  return sqlite3_errcode(database) == SQLITE_OK
             ? std::make_error_code(std::errc::invalid_argument)
             : sqliteError(database);
}

class BulkScope {
public:
  static std::expected<BulkScope, std::error_code> start(Connection connection,
                                                         bool atomic) {
    if (!atomic) {
      return BulkScope(std::move(connection));
    }
    if (sqlite3_get_autocommit(connection.get()) == 0) {
      return execute(connection.get(), "SAVEPOINT facts_bulk")
          .transform([connection] { return BulkScope(connection, true); });
    }
    return Transaction::immediate(connection)
        .transform([connection](Transaction transaction) mutable {
          return BulkScope(std::move(connection), std::move(transaction));
        });
  }

  BulkScope(BulkScope &&other) noexcept
      : connection_(std::move(other.connection_)),
        transaction_(std::move(other.transaction_)),
        savepoint_(std::exchange(other.savepoint_, false)),
        active_(std::exchange(other.active_, false)) {}

  BulkScope &operator=(BulkScope &&) = delete;
  BulkScope(const BulkScope &) = delete;
  BulkScope &operator=(const BulkScope &) = delete;

  ~BulkScope() {
    if (!active_) {
      return;
    }
    if (transaction_) {
      (void)transaction_->rollback();
      return;
    }
    if (savepoint_) {
      (void)execute(connection_.get(),
                    "ROLLBACK TO facts_bulk; RELEASE facts_bulk");
    }
  }

  std::expected<void, std::error_code> complete() {
    if (!active_) {
      return {};
    }
    auto completed = transaction_
                         ? transaction_->commit()
                         : execute(connection_.get(), "RELEASE facts_bulk");
    if (completed) {
      active_ = false;
    }
    return completed;
  }

private:
  explicit BulkScope(Connection connection)
      : connection_(std::move(connection)), active_(false) {}

  BulkScope(Connection connection, bool savepoint)
      : connection_(std::move(connection)), savepoint_(savepoint) {}

  BulkScope(Connection connection, Transaction transaction)
      : connection_(std::move(connection)),
        transaction_(std::move(transaction)) {}

  Connection connection_;
  std::optional<Transaction> transaction_;
  bool savepoint_{false};
  bool active_{true};
};

template <typename Binder, typename Reference>
concept BulkBinder =
    requires(Binder &binder, sqlite3_stmt *statement, Reference value) {
      { std::invoke(binder, statement, value) } -> std::convertible_to<bool>;
    };

template <std::ranges::input_range Input, typename Binder>
  requires BulkBinder<Binder, std::ranges::range_reference_t<Input>>
std::expected<BulkResult, std::error_code>
runPreparedBulk(sqlite3 *database, sqlite3_stmt *statement, Input &&input,
                Binder &&binder) {
  BulkResult result{};
  bool first = true;
  for (auto &&value : input) {
    if (!first) {
      if (sqlite3_reset(statement) != SQLITE_OK ||
          sqlite3_clear_bindings(statement) != SQLITE_OK) {
        return std::unexpected(sqliteError(database));
      }
    }
    first = false;

    if (!std::invoke(binder, statement, value)) {
      return std::unexpected(bindingError(database));
    }

    int status = SQLITE_ROW;
    while (status == SQLITE_ROW) {
      status = sqlite3_step(statement);
    }
    if (status != SQLITE_DONE) {
      return std::unexpected(sqliteError(database));
    }

    const std::int64_t changes = directChangeCount(database);
    if (changes > 0 &&
        result.changes > std::numeric_limits<std::int64_t>::max() - changes) {
      return std::unexpected(std::make_error_code(std::errc::value_too_large));
    }
    ++result.processed;
    result.changes += changes;
  }
  return result;
}

} // namespace detail

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

  std::expected<CommandResult, std::error_code>
  executeCommand(std::string_view sql) {
    return storage::executeCommand(handle(), sql);
  }

  std::expected<void, std::error_code> execute(std::string_view sql) {
    return executeCommand(sql).transform([](const CommandResult &) {});
  }

  std::expected<void, std::error_code> executeScript(std::string_view sql) {
    return storage::execute(handle(), sql);
  }

  template <std::ranges::input_range Input, typename Binder>
    requires detail::BulkBinder<Binder, std::ranges::range_reference_t<Input>>
  std::expected<BulkResult, std::error_code>
  executeBulk(std::string_view sql, Input &&input, Binder &&binder,
              BulkOptions options = {}) {
    return detail::BulkScope::start(connection_, options.atomic)
        .and_then([&](detail::BulkScope scope) {
          return prepareSingle(handle(), sql)
              .and_then([&](Statement statement) {
                return detail::runPreparedBulk(handle(), statement.get(),
                                               std::forward<Input>(input),
                                               std::forward<Binder>(binder))
                    .and_then([&](BulkResult result) {
                      return scope.complete().transform(
                          [result] { return result; });
                    });
              });
        });
  }

  template <typename Binder>
  auto bulk(std::string sql, Binder &&binder, BulkOptions options = {});

  std::expected<Transaction, std::error_code>
  transaction(TransactionMode mode = TransactionMode::deferred) {
    switch (mode) {
    case TransactionMode::deferred:
      return Transaction::deferred(connection_);
    case TransactionMode::immediate:
      return Transaction::immediate(connection_);
    case TransactionMode::exclusive:
      return Transaction::exclusive(connection_);
    }
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  std::expected<Transaction, std::error_code> read() {
    return transaction(TransactionMode::deferred);
  }

  std::expected<Transaction, std::error_code> write() {
    return transaction(TransactionMode::immediate);
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

template <typename Binder>
class BulkTerminal {
public:
  BulkTerminal(Database &database, std::string sql, Binder binder,
               BulkOptions options)
      : database_(&database), sql_(std::move(sql)), binder_(std::move(binder)),
        options_(options) {}

  template <std::ranges::input_range Input>
    requires detail::BulkBinder<Binder, std::ranges::range_reference_t<Input>>
  friend std::expected<BulkResult, std::error_code>
  operator|(Input &&input, BulkTerminal &&terminal) {
    return terminal.database_->executeBulk(terminal.sql_,
                                           std::forward<Input>(input),
                                           terminal.binder_, terminal.options_);
  }

private:
  Database *database_;
  std::string sql_;
  Binder binder_;
  BulkOptions options_;
};

template <typename Binder>
auto Database::bulk(std::string sql, Binder &&binder, BulkOptions options) {
  return BulkTerminal<std::decay_t<Binder>>(
      *this, std::move(sql), std::forward<Binder>(binder), options);
}

} // namespace facts::storage

#endif // FACTS_TOOL_STORAGE_SQLITE_DATABASE_H
