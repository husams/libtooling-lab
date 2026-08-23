#ifndef FACTS_TOOL_STORAGE_SQLITE_H
#define FACTS_TOOL_STORAGE_SQLITE_H

#include "model/SymbolId.h"

#include <sqlite3.h>

#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace facts::storage {

using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

namespace detail {

using Connection = std::shared_ptr<sqlite3>;

std::int64_t widenLegacyChangeCount(int changes) noexcept;
std::int64_t directChangeCount(sqlite3 *database) noexcept;

} // namespace detail

struct CommandResult {
  std::int64_t changes;

  bool operator==(const CommandResult &) const = default;
};

enum class TransactionMode { deferred, immediate, exclusive };
enum class TransactionState { active, committed, rolledBack };

std::error_code sqliteError(sqlite3 *database) noexcept;

std::expected<Statement, std::error_code> prepare(sqlite3 *database,
                                                  std::string_view sql);

std::expected<Statement, std::error_code> prepareSingle(sqlite3 *database,
                                                        std::string_view sql);

std::expected<void, std::error_code> execute(sqlite3 *database,
                                             const char *sql) noexcept;

std::expected<void, std::error_code> execute(sqlite3 *database,
                                             std::string_view sql);

std::expected<CommandResult, std::error_code>
executeCommand(sqlite3 *database, std::string_view sql);

bool bindText(sqlite3_stmt *statement, int position, std::string_view value);

template <typename Value>
bool bindInteger(sqlite3_stmt *statement, int position, Value value) {
  if constexpr (std::is_enum_v<Value>) {
    return sqlite3_bind_int64(statement, position,
                              static_cast<sqlite3_int64>(
                                  static_cast<std::underlying_type_t<Value>>(
                                      value))) == SQLITE_OK;
  }
  return sqlite3_bind_int64(statement, position,
                            static_cast<sqlite3_int64>(value)) == SQLITE_OK;
}

std::string columnText(sqlite3_stmt *statement, int column);

std::uint64_t packSymbolId(SymbolId id);
SymbolId unpackSymbolId(std::uint64_t packed);

class Transaction {
public:
  static std::expected<Transaction, std::error_code>
  deferred(detail::Connection database);
  static std::expected<Transaction, std::error_code>
  immediate(detail::Connection database);
  static std::expected<Transaction, std::error_code>
  exclusive(detail::Connection database);

  static std::expected<Transaction, std::error_code>
  deferred(sqlite3 *database);
  static std::expected<Transaction, std::error_code>
  immediate(sqlite3 *database);
  static std::expected<Transaction, std::error_code>
  exclusive(sqlite3 *database);

  static std::expected<Transaction, std::error_code> read(sqlite3 *database);
  static std::expected<Transaction, std::error_code> write(sqlite3 *database);

  Transaction(Transaction &&other) noexcept;
  Transaction &operator=(Transaction &&other) noexcept;
  Transaction(const Transaction &) = delete;
  Transaction &operator=(const Transaction &) = delete;

  ~Transaction() noexcept;

  std::expected<void, std::error_code> commit();
  std::expected<void, std::error_code> rollback() noexcept;

  TransactionState state() const noexcept { return state_; }

  bool active() const noexcept { return state_ == TransactionState::active; }

private:
  explicit Transaction(detail::Connection database);

  static std::expected<Transaction, std::error_code>
  begin(detail::Connection database, TransactionMode mode);

  static detail::Connection borrow(sqlite3 *database);
  static const char *command(TransactionMode mode) noexcept;
  std::expected<void, std::error_code> terminalError() const;

  detail::Connection database_;
  TransactionState state_{TransactionState::active};
};

} // namespace facts::storage

#endif
