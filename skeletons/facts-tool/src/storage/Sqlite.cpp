#include "storage/Sqlite.h"

#include <utility>

namespace facts::storage {

std::error_code sqliteError(sqlite3 *database) noexcept {
  return {sqlite3_extended_errcode(database), std::generic_category()};
}

std::expected<Statement, std::error_code> prepare(sqlite3 *database,
                                                  std::string_view sql) {
  sqlite3_stmt *raw = nullptr;
  if (sqlite3_prepare_v2(database, sql.data(), static_cast<int>(sql.size()),
                         &raw, nullptr) != SQLITE_OK) {
    return std::unexpected(sqliteError(database));
  }
  return Statement(raw, sqlite3_finalize);
}

std::expected<Statement, std::error_code> prepareSingle(sqlite3 *database,
                                                        std::string_view sql) {
  sqlite3_stmt *raw = nullptr;
  const char *tail = nullptr;
  const int status = sqlite3_prepare_v2(
      database, sql.data(), static_cast<int>(sql.size()), &raw, &tail);
  if (status != SQLITE_OK) {
    return std::unexpected(sqliteError(database));
  }
  Statement statement(raw, sqlite3_finalize);
  if (!statement) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  const char *end = sql.data() + sql.size();
  while (tail < end) {
    sqlite3_stmt *extraRaw = nullptr;
    const char *next = nullptr;
    const int extraStatus = sqlite3_prepare_v2(
        database, tail, static_cast<int>(end - tail), &extraRaw, &next);
    Statement extra(extraRaw, sqlite3_finalize);
    if (extraStatus != SQLITE_OK) {
      return std::unexpected(sqliteError(database));
    }
    if (extra) {
      return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }
    if (!next || next == tail) {
      break;
    }
    tail = next;
  }
  return statement;
}

std::expected<void, std::error_code> execute(sqlite3 *database,
                                             std::string_view sql) {
  const std::string command(sql);
  return execute(database, command.c_str());
}

std::expected<void, std::error_code> execute(sqlite3 *database,
                                             const char *sql) noexcept {
  if (sqlite3_exec(database, sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
    return std::unexpected(sqliteError(database));
  }
  return {};
}

namespace detail {

std::int64_t widenLegacyChangeCount(int changes) noexcept {
  return static_cast<std::int64_t>(changes);
}

std::int64_t directChangeCount(sqlite3 *database) noexcept {
#if SQLITE_VERSION_NUMBER >= 3037000
  return sqlite3_changes64(database);
#else
  return widenLegacyChangeCount(sqlite3_changes(database));
#endif
}

} // namespace detail

std::expected<CommandResult, std::error_code>
executeCommand(sqlite3 *database, std::string_view sql) {
  const int totalBefore = sqlite3_total_changes(database);
  return prepareSingle(database, sql)
      .and_then([database, totalBefore](Statement statement)
                    -> std::expected<CommandResult, std::error_code> {
        while (true) {
          const int status = sqlite3_step(statement.get());
          if (status == SQLITE_DONE) {
            return CommandResult{sqlite3_total_changes(database) == totalBefore
                                     ? 0
                                     : detail::directChangeCount(database)};
          }
          if (status != SQLITE_ROW) {
            return std::unexpected(sqliteError(database));
          }
        }
      });
}

bool bindText(sqlite3_stmt *statement, int position, std::string_view value) {
  return sqlite3_bind_text(statement, position, value.data(),
                           static_cast<int>(value.size()),
                           SQLITE_TRANSIENT) == SQLITE_OK;
}

std::string columnText(sqlite3_stmt *statement, int column) {
  const auto *value = sqlite3_column_text(statement, column);
  return value ? std::string{reinterpret_cast<const char *>(value),
                             static_cast<std::size_t>(
                                 sqlite3_column_bytes(statement, column))}
               : std::string{};
}

std::uint64_t packSymbolId(SymbolId id) { return id.packed(); }

SymbolId unpackSymbolId(std::uint64_t packed) {
  return {
      static_cast<FileId>(packed >> 32U),
      static_cast<std::uint32_t>(packed),
  };
}

std::expected<Transaction, std::error_code>
Transaction::read(sqlite3 *database) {
  return deferred(database);
}

std::expected<Transaction, std::error_code>
Transaction::write(sqlite3 *database) {
  return immediate(database);
}

std::expected<Transaction, std::error_code>
Transaction::deferred(detail::Connection database) {
  return begin(std::move(database), TransactionMode::deferred);
}

std::expected<Transaction, std::error_code>
Transaction::immediate(detail::Connection database) {
  return begin(std::move(database), TransactionMode::immediate);
}

std::expected<Transaction, std::error_code>
Transaction::exclusive(detail::Connection database) {
  return begin(std::move(database), TransactionMode::exclusive);
}

std::expected<Transaction, std::error_code>
Transaction::deferred(sqlite3 *database) {
  return deferred(borrow(database));
}

std::expected<Transaction, std::error_code>
Transaction::immediate(sqlite3 *database) {
  return immediate(borrow(database));
}

std::expected<Transaction, std::error_code>
Transaction::exclusive(sqlite3 *database) {
  return exclusive(borrow(database));
}

Transaction::Transaction(Transaction &&other) noexcept
    : database_(std::move(other.database_)),
      state_(std::exchange(other.state_, TransactionState::rolledBack)) {}

Transaction &Transaction::operator=(Transaction &&other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (active()) {
    (void)rollback();
  }
  database_ = std::move(other.database_);
  state_ = std::exchange(other.state_, TransactionState::rolledBack);
  return *this;
}

Transaction::~Transaction() noexcept {
  if (active()) {
    (void)rollback();
  }
}

std::expected<void, std::error_code> Transaction::commit() {
  if (!active()) {
    return terminalError();
  }
  auto committed = execute(database_.get(), "COMMIT");
  if (committed) {
    state_ = TransactionState::committed;
  } else if (sqlite3_get_autocommit(database_.get()) != 0) {
    state_ = TransactionState::rolledBack;
  }
  return committed;
}

std::expected<void, std::error_code> Transaction::rollback() noexcept {
  if (!active()) {
    return terminalError();
  }
  auto rolledBack = execute(database_.get(), "ROLLBACK");
  if (rolledBack || sqlite3_get_autocommit(database_.get()) != 0) {
    state_ = TransactionState::rolledBack;
  }
  return rolledBack;
}

Transaction::Transaction(detail::Connection database)
    : database_(std::move(database)) {}

std::expected<Transaction, std::error_code>
Transaction::begin(detail::Connection database, TransactionMode mode) {
  return execute(database.get(), command(mode)).transform([database] {
    return Transaction(std::move(database));
  });
}

detail::Connection Transaction::borrow(sqlite3 *database) {
  return detail::Connection(database, [](sqlite3 *) {});
}

const char *Transaction::command(TransactionMode mode) noexcept {
  switch (mode) {
  case TransactionMode::deferred:
    return "BEGIN";
  case TransactionMode::immediate:
    return "BEGIN IMMEDIATE";
  case TransactionMode::exclusive:
    return "BEGIN EXCLUSIVE";
  }
  return "BEGIN";
}

std::expected<void, std::error_code> Transaction::terminalError() const {
  return std::unexpected(
      std::make_error_code(std::errc::operation_not_permitted));
}

} // namespace facts::storage
