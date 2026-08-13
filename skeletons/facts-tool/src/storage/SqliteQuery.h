#ifndef FACTS_TOOL_STORAGE_SQLITE_QUERY_H
#define FACTS_TOOL_STORAGE_SQLITE_QUERY_H

// The cursor half of the coroutine query API: row access, parameter binding,
// and the two generators storage::Database hands out. It is built on the
// primitives in Sqlite.h -- prepare(), bindText(), columnText(), sqliteError()
// -- and adds no SQLite handling of its own. Nothing here is meant to be
// called directly; go through Database, which owns the connection.
//
// Two lifetime rules the coroutine imposes:
//   * The sql string and the bind arguments are taken *by value*, so they are
//     copied into the frame. Reference parameters would dangle, because the
//     body does not start running until the first iteration.
//   * A Row is a view of the cursor's current position, and text() points into
//     SQLite's own buffer. Both are invalidated by the next step, which is why
//     Database::query() maps each row to an owned value while it is still
//     alive -- and why that is the form to reach for when composing views.
//
// Errors are thrown as QueryError while iterating, so a stream stays a stream
// of plain values and pipes into std::ranges cleanly. collect() turns that
// back into the std::expected the rest of the storage layer speaks.

#include "storage/Generator.h"
#include "storage/Sqlite.h"

#include <sqlite3.h>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace facts::storage {

class QueryError : public std::system_error {
public:
  QueryError(std::error_code code, const std::string &message)
      : std::system_error(code, message) {}
};

[[noreturn]] inline void raiseQueryError(sqlite3 *database) {
  throw QueryError(sqliteError(database), sqlite3_errmsg(database));
}

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

  // Valid until the next step; use string() if it has to outlive the loop body.
  std::string_view text(int column) const noexcept {
    const auto *bytes = sqlite3_column_text(statement_, column);
    if (bytes == nullptr) {
      return {};
    }
    return {reinterpret_cast<const char *>(bytes),
            static_cast<std::size_t>(sqlite3_column_bytes(statement_, column))};
  }

  std::string string(int column) const {
    return columnText(statement_, column);
  }

private:
  sqlite3_stmt *statement_;
};

namespace detail {

// Bind parameters are 1-based. Constrained overloads keep an `int` argument
// from being ambiguous between the integer and floating-point forms.
template <typename Value>
  requires std::integral<Value> || std::is_enum_v<Value>
bool bindOne(sqlite3_stmt *statement, int position, Value value) {
  return bindInteger(statement, position, value);
}

template <std::floating_point Value>
bool bindOne(sqlite3_stmt *statement, int position, Value value) {
  return sqlite3_bind_double(statement, position,
                             static_cast<double>(value)) == SQLITE_OK;
}

inline bool bindOne(sqlite3_stmt *statement, int position,
                    std::string_view value) {
  return bindText(statement, position, value);
}

inline bool bindOne(sqlite3_stmt *statement, int position, std::nullptr_t) {
  return sqlite3_bind_null(statement, position) == SQLITE_OK;
}

inline bool bindOne(sqlite3_stmt *statement, int position, std::nullopt_t) {
  return sqlite3_bind_null(statement, position) == SQLITE_OK;
}

// A symbol id is one 64-bit key on the wire, the same packing the schema uses.
inline bool bindOne(sqlite3_stmt *statement, int position, SymbolId id) {
  return bindInteger(statement, position, packSymbolId(id));
}

// The customization point: a model type binds itself by declaring
//
//   bool bindValue(sqlite3_stmt *, int, const T &);
//
// in its own namespace. ADL finds it here, so nothing in this header has to
// know about the type.
template <typename Value>
concept SelfBinding = requires(sqlite3_stmt *statement, int position,
                               const Value &value) {
  { bindValue(statement, position, value) } -> std::same_as<bool>;
};

template <SelfBinding Value>
bool bindOne(sqlite3_stmt *statement, int position, const Value &value) {
  return bindValue(statement, position, value);
}

// Declared last so the overloads above are all visible to the recursive call:
// an engaged optional binds as its underlying type, an empty one as NULL.
template <typename Value>
bool bindOne(sqlite3_stmt *statement, int position,
             const std::optional<Value> &value) {
  return value ? bindOne(statement, position, *value)
               : sqlite3_bind_null(statement, position) == SQLITE_OK;
}

// Anything one of the overloads above accepts can be passed as a parameter.
// Constraining the pack turns a wrong argument type into one short error at
// the call site instead of a template instantiation dump.
template <typename Value>
concept Bindable = requires(sqlite3_stmt *statement, int position,
                            const Value &value) {
  { bindOne(statement, position, value) } -> std::same_as<bool>;
};

// Streams a result set. Nothing is prepared until the first iteration, and the
// statement is finalized when the generator dies -- including on an early
// break, or when an exception unwinds the loop.
template <Bindable... Binds>
Generator<Row> rowStream(sqlite3 *database, std::string sql, Binds... binds) {
  auto statement = prepare(database, sql);
  if (!statement) {
    raiseQueryError(database);
  }

  // Catch `?2` in the sql with one argument passed, which SQLite would other-
  // wise bind to NULL and answer with a silently empty result set.
  if (sqlite3_bind_parameter_count(statement->get()) !=
      static_cast<int>(sizeof...(Binds))) {
    throw QueryError(std::make_error_code(std::errc::invalid_argument),
                     "expected " +
                         std::to_string(sqlite3_bind_parameter_count(
                             statement->get())) +
                         " bind argument(s), got " +
                         std::to_string(sizeof...(Binds)) + ": " + sql);
  }

  int position = 0;
  const std::array<bool, sizeof...(Binds)> bound{
      bindOne(statement->get(), ++position, binds)...};
  for (const bool ok : bound) {
    if (!ok) {
      raiseQueryError(database);
    }
  }

  Row row(statement->get());
  while (true) {
    const int status = sqlite3_step(statement->get());
    if (status == SQLITE_DONE) {
      co_return;
    }
    if (status != SQLITE_ROW) {
      raiseQueryError(database);
    }
    co_yield row;
  }
}

// Same stream, mapped while the row is still alive, so what comes out owns
// itself and survives the next step.
template <typename Map, Bindable... Binds>
auto valueStream(sqlite3 *database, std::string sql, Map map, Binds... binds)
    -> Generator<std::invoke_result_t<Map &, const Row &>> {
  for (const Row &row :
       rowStream(database, std::move(sql), std::move(binds)...)) {
    co_yield std::invoke(map, row);
  }
}

} // namespace detail

// Drains a stream into one vector, converting QueryError back into the
// std::expected the storage layer uses at its boundaries.
template <std::ranges::input_range Rows>
auto collect(Rows &&rows)
    -> std::expected<std::vector<std::ranges::range_value_t<Rows>>,
                     std::error_code> {
  static_assert(!std::is_same_v<std::ranges::range_value_t<Rows>, Row>,
                "collecting rows() would store views of a statement that is "
                "finalized on the way out -- map them with query() instead");
  std::vector<std::ranges::range_value_t<Rows>> values;
  try {
    for (auto &&value : rows) {
      values.push_back(std::forward<decltype(value)>(value));
    }
  } catch (const QueryError &error) {
    return std::unexpected(error.code());
  }
  return values;
}

} // namespace facts::storage

#endif // FACTS_TOOL_STORAGE_SQLITE_QUERY_H
