#pragma once

#include "storage/SqliteDatabase.h"
#include <filesystem>
#include <string>
#include <vector>

namespace facts::catalog {
using Database = storage::Database;
template <typename T>
using Result = std::expected<T, std::string>;

Result<Database> open(const std::string &path, bool writable,
                      bool create = false);
Result<std::filesystem::path> existingDirectory(const std::string &path);
Result<void> validateVersion(const std::string &version);

inline std::string databaseError(Database &database) {
  return sqlite3_errmsg(database.nativeHandle());
}

template <typename Map, typename... Binds>
auto query(Database &database, std::string sql, Map map, Binds &&...binds)
    -> Result<std::vector<std::invoke_result_t<Map &, const storage::Row &>>> {
  using Value = std::invoke_result_t<Map &, const storage::Row &>;
  try {
    std::vector<Value> values;
    for (auto value : database.query(std::move(sql), std::move(map),
                                     std::forward<Binds>(binds)...)) {
      values.push_back(std::move(value));
    }
    return values;
  } catch (const storage::QueryError &error) {
    return std::unexpected(error.what());
  }
}

template <typename... Binds>
Result<void> execute(Database &database, std::string_view sql,
                     const Binds &...binds) {
  return storage::prepareSingle(database.nativeHandle(), sql)
      .transform_error([&](auto) { return databaseError(database); })
      .and_then([&](storage::Statement statement) -> Result<void> {
        if (!storage::bindParameters(statement.get(), binds...)) {
          return std::unexpected(databaseError(database));
        }
        if (sqlite3_step(statement.get()) != SQLITE_DONE) {
          return std::unexpected(databaseError(database));
        }
        return {};
      });
}

template <typename T>
Result<T> requireOne(std::vector<T> values, std::string_view description) {
  if (values.empty())
    return std::unexpected(std::string(description) + " not found");
  if (values.size() != 1)
    return std::unexpected("ambiguous " + std::string(description));
  return std::move(values.front());
}

template <typename Work>
auto transaction(Database &database, Work work) -> decltype(work()) {
  return database.write()
      .transform_error([&](auto) { return databaseError(database); })
      .and_then([&](storage::Transaction scope) {
        return work().and_then([&](auto result) {
          return scope.commit()
              .transform_error([&](auto) { return databaseError(database); })
              .transform([&] { return std::move(result); });
        });
      });
}
} // namespace facts::catalog
