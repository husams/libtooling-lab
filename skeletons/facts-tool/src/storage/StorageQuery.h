#ifndef FACTS_TOOL_STORAGE_STORAGE_QUERY_H
#define FACTS_TOOL_STORAGE_STORAGE_QUERY_H

#include "storage/SqliteQuery.h"

#include <itlib/generator.hpp>

#include <expected>
#include <functional>
#include <optional>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace facts::storage::detail {

class ParameterBinder {
public:
  explicit ParameterBinder(sqlite3_stmt *statement) : statement_(statement) {}

  template <typename... Values>
  bool operator()(Values &&...values) const {
    return storage::bindParameters(statement_, std::forward<Values>(values)...);
  }

private:
  sqlite3_stmt *statement_;
};

template <typename Bind>
auto typedBinder(Bind bind) {
  return
      [bind = std::move(bind)](sqlite3_stmt *statement, auto &&value) mutable {
        return std::invoke(bind, ParameterBinder{statement},
                           std::forward<decltype(value)>(value));
      };
}

template <typename Value>
auto collectGenerator(itlib::generator<Value> values)
    -> std::expected<std::vector<std::remove_cvref_t<Value>>, std::error_code> {
  std::vector<std::remove_cvref_t<Value>> collected;
  try {
    for (auto &&value : values) {
      collected.push_back(std::forward<decltype(value)>(value));
    }
  } catch (const QueryError &error) {
    return std::unexpected(error.code());
  }
  return collected;
}

template <typename Value>
auto collectOptional(itlib::generator<Value> values)
    -> std::expected<std::optional<std::remove_cvref_t<Value>>,
                     std::error_code> {
  return collectGenerator(std::move(values))
      .and_then([](auto collected)
                    -> std::expected<std::optional<std::remove_cvref_t<Value>>,
                                     std::error_code> {
        if (collected.size() > 1) {
          return std::unexpected(
              std::make_error_code(std::errc::result_out_of_range));
        }
        return collected.empty() ? std::optional<std::remove_cvref_t<Value>>{}
                                 : std::optional<std::remove_cvref_t<Value>>{
                                       std::move(collected.front())};
      });
}

template <typename Value>
auto collectOne(itlib::generator<Value> values)
    -> std::expected<std::remove_cvref_t<Value>, std::error_code> {
  return collectOptional(std::move(values))
      .and_then(
          [](auto value)
              -> std::expected<std::remove_cvref_t<Value>, std::error_code> {
            if (!value) {
              return std::unexpected(
                  std::make_error_code(std::errc::no_such_file_or_directory));
            }
            return std::move(*value);
          });
}

} // namespace facts::storage::detail

#endif
