#ifndef FACTS_TOOL_STORAGE_SQLITE_QUERY_H
#define FACTS_TOOL_STORAGE_SQLITE_QUERY_H

// A coroutine front end for SQLite queries.
//
// The C API is a cursor: prepare, bind, step until SQLITE_DONE, finalize.
// Every caller that wants rows has to reimplement that loop and remember to
// finalize on each exit path. rows() and select() move the cursor into a
// coroutine frame and hand back a lazy input range instead, so callers write
//
//   for (auto row : sql::rows(db, "SELECT id, path FROM file")) ...
//
// and the statement is finalized when the range dies -- including on an early
// break, or when an exception unwinds the loop.
//
// Errors stay values: the element type is std::expected, matching FactStore
// and FileDatabase. The first failure is yielded once and ends the stream.
//
// Two lifetime rules the coroutine imposes:
//   * The sql string and the bind arguments are taken *by value*, so they are
//     copied into the frame. Reference parameters would dangle, because the
//     body does not start running until the first iteration.
//   * A Row is a view of the cursor's current position, and text() points into
//     SQLite's own buffer. Both are invalidated by the next step. Copy out
//     anything that must outlive the loop body -- that is what select() does.

#include "storage/Generator.h"
#include "storage/SqliteConnection.h"

#include <sqlite3.h>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace facts::sql {

using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

// A non-owning view of the row the cursor currently sits on.
class Row {
public:
  explicit Row(sqlite3_stmt *statement) noexcept : statement_(statement) {}

  int columns() const noexcept { return sqlite3_column_count(statement_); }

  bool isNull(int column) const noexcept {
    return sqlite3_column_type(statement_, column) == SQLITE_NULL;
  }

  std::int64_t integer(int column) const noexcept {
    return sqlite3_column_int64(statement_, column);
  }

  double real(int column) const noexcept {
    return sqlite3_column_double(statement_, column);
  }

  // Valid until the next step; copy it if it has to outlive the loop body.
  std::string_view text(int column) const noexcept {
    const auto *bytes = sqlite3_column_text(statement_, column);
    if (bytes == nullptr) {
      return {};
    }
    return {reinterpret_cast<const char *>(bytes),
            static_cast<std::size_t>(sqlite3_column_bytes(statement_, column))};
  }

  std::string string(int column) const { return std::string(text(column)); }

private:
  sqlite3_stmt *statement_;
};

// Bind parameters are 1-based. Constrained overloads keep an `int` argument
// from being ambiguous between the integer and floating-point forms.
template <std::integral Value>
int bindOne(sqlite3_stmt *statement, int index, Value value) {
  return sqlite3_bind_int64(statement, index,
                            static_cast<sqlite3_int64>(value));
}

template <std::floating_point Value>
int bindOne(sqlite3_stmt *statement, int index, Value value) {
  return sqlite3_bind_double(statement, index, static_cast<double>(value));
}

inline int bindOne(sqlite3_stmt *statement, int index, std::string_view value) {
  return sqlite3_bind_text(statement, index, value.data(),
                           static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

inline int bindOne(sqlite3_stmt *statement, int index, std::nullptr_t) {
  return sqlite3_bind_null(statement, index);
}

// Streams a result set. Nothing is prepared until the first iteration.
template <typename... Binds>
Generator<std::expected<Row, std::error_code>>
rows(sqlite3 *database, std::string sql, Binds... binds) {
  sqlite3_stmt *raw = nullptr;
  if (sqlite3_prepare_v2(database, sql.c_str(),
                         static_cast<int>(sql.size()) + 1, &raw,
                         nullptr) != SQLITE_OK) {
    co_yield std::unexpected(lastError(database));
    co_return;
  }
  const Statement statement(raw, sqlite3_finalize);

  int index = 0;
  const std::array<int, sizeof...(Binds)> bindStatuses{
      bindOne(statement.get(), ++index, binds)...};
  for (const int status : bindStatuses) {
    if (status != SQLITE_OK) {
      co_yield std::unexpected(lastError(database));
      co_return;
    }
  }

  const Row row(statement.get());
  while (true) {
    const int status = sqlite3_step(statement.get());
    if (status == SQLITE_DONE) {
      co_return;
    }
    if (status != SQLITE_ROW) {
      co_yield std::unexpected(lastError(database));
      co_return;
    }
    co_yield row;
  }
}

// Same stream, mapped through `map` while the row is still alive, so callers
// never see a sqlite3 type or a dangling string_view.
template <typename Map, typename... Binds>
auto select(sqlite3 *database, std::string sql, Map map, Binds... binds)
    -> Generator<std::expected<std::invoke_result_t<Map &, const Row &>,
                               std::error_code>> {
  for (const auto &result : rows(database, std::move(sql), std::move(binds)...)) {
    if (!result) {
      co_yield std::unexpected(result.error());
      co_return;
    }
    co_yield std::invoke(map, *result);
  }
}

// Drains a stream of expected values into one expected vector, stopping at the
// first error -- the eager counterpart for callers that do want everything.
template <std::ranges::input_range Results>
auto collect(Results &&results) -> std::expected<
    std::vector<typename std::ranges::range_value_t<Results>::value_type>,
    typename std::ranges::range_value_t<Results>::error_type> {
  using Result = std::ranges::range_value_t<Results>;
  std::vector<typename Result::value_type> values;
  for (const auto &result : results) {
    if (!result) {
      return std::unexpected<typename Result::error_type>(result.error());
    }
    values.push_back(*result);
  }
  return values;
}

} // namespace facts::sql

#endif // FACTS_TOOL_STORAGE_SQLITE_QUERY_H
