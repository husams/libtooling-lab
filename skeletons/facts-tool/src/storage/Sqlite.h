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

std::error_code sqliteError(sqlite3 *database);

std::expected<Statement, std::error_code> prepare(sqlite3 *database,
                                                  std::string_view sql);

std::expected<void, std::error_code> execute(sqlite3 *database,
                                             std::string_view sql);

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
  static std::expected<Transaction, std::error_code> read(sqlite3 *database);
  static std::expected<Transaction, std::error_code> write(sqlite3 *database);

  Transaction(Transaction &&other) noexcept;
  Transaction &operator=(Transaction &&) = delete;
  Transaction(const Transaction &) = delete;
  Transaction &operator=(const Transaction &) = delete;

  ~Transaction();

  std::expected<void, std::error_code> commit();

private:
  explicit Transaction(sqlite3 *database);

  static std::expected<Transaction, std::error_code>
  begin(sqlite3 *database, std::string_view command);

  sqlite3 *database_;
};

} // namespace facts::storage

#endif
