#include "storage/Sqlite.h"

#include <utility>

namespace facts::storage {

std::error_code sqliteError(sqlite3 *database) {
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

std::expected<void, std::error_code> execute(sqlite3 *database,
                                             std::string_view sql) {
  const std::string command(sql);
  if (sqlite3_exec(database, command.c_str(), nullptr, nullptr, nullptr) !=
      SQLITE_OK) {
    return std::unexpected(sqliteError(database));
  }
  return {};
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

std::expected<Transaction, std::error_code>
Transaction::read(sqlite3 *database) {
  return begin(database, "BEGIN");
}

std::expected<Transaction, std::error_code>
Transaction::write(sqlite3 *database) {
  return begin(database, "BEGIN IMMEDIATE");
}

Transaction::Transaction(Transaction &&other) noexcept
    : database_(std::exchange(other.database_, nullptr)) {}

Transaction::~Transaction() {
  if (database_) {
    execute(database_, "ROLLBACK");
  }
}

std::expected<void, std::error_code> Transaction::commit() {
  return execute(database_, "COMMIT").transform([this] {
    database_ = nullptr;
  });
}

Transaction::Transaction(sqlite3 *database) : database_(database) {}

std::expected<Transaction, std::error_code>
Transaction::begin(sqlite3 *database, std::string_view command) {
  return execute(database, command).transform([database] {
    return Transaction(database);
  });
}

} // namespace facts::storage
