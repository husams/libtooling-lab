#include "StorageSchemaTestSupport.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <ranges>

namespace storage_schema_test {

bool require(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

bool execute(sqlite3 *database, std::string_view sql) {
  char *message = nullptr;
  const auto result =
      sqlite3_exec(database, sql.data(), nullptr, nullptr, &message);
  if (result == SQLITE_OK) {
    return true;
  }
  std::cerr << (message ? message : sqlite3_errmsg(database)) << '\n';
  sqlite3_free(message);
  return false;
}

std::int64_t scalar(sqlite3 *database, std::string_view sql) {
  sqlite3_stmt *statement = nullptr;
  if (sqlite3_prepare_v2(database, sql.data(), -1, &statement, nullptr) !=
      SQLITE_OK) {
    return -1;
  }
  const auto result = sqlite3_step(statement) == SQLITE_ROW
                          ? sqlite3_column_int64(statement, 0)
                          : -1;
  sqlite3_finalize(statement);
  return result;
}

std::string textScalar(sqlite3 *database, std::string_view sql) {
  sqlite3_stmt *statement = nullptr;
  if (sqlite3_prepare_v2(database, sql.data(), -1, &statement, nullptr) !=
      SQLITE_OK) {
    return {};
  }
  std::string value;
  if (sqlite3_step(statement) == SQLITE_ROW) {
    const auto *text = sqlite3_column_text(statement, 0);
    value = text ? reinterpret_cast<const char *>(text) : std::string{};
  }
  sqlite3_finalize(statement);
  return value;
}

std::uint64_t packed(facts::SymbolId id) {
  return (static_cast<std::uint64_t>(id.file) << 32U) | id.index;
}

bool noPackedFlags(sqlite3 *database) {
  constexpr std::array tables{"symbol", "parameter", "relation",
                              "template_argument", "template_parameter"};
  return std::ranges::all_of(tables, [&](std::string_view table) {
    return scalar(database, "SELECT COUNT(*) FROM pragma_table_info('" +
                                std::string{table} + "') WHERE name='flags'") ==
           0;
  });
}

bool noRedundantSymbolIdColumns(sqlite3 *database) {
  return scalar(database,
                "SELECT COUNT(*) FROM pragma_table_info('symbol') WHERE "
                "name IN ('file_id','file_index')") == 0;
}

bool usrIsOnlySymbolIdentity(sqlite3 *database) {
  return scalar(database,
                "SELECT COUNT(*) FROM pragma_table_info('symbol') WHERE "
                "name='identity'") == 0;
}

void removeDatabase(const std::filesystem::path &path) {
  std::filesystem::remove(path);
}

} // namespace storage_schema_test
