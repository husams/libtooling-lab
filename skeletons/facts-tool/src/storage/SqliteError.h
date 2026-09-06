#pragma once

#include <sqlite3.h>
#include <string>
#include <system_error>

namespace facts::storage {

inline const std::error_category &sqliteCategory() noexcept {
  class Category final : public std::error_category {
  public:
    const char *name() const noexcept override { return "sqlite"; }

    std::string message(int code) const override {
      return std::string(sqlite3_errstr(code)) + " (SQLite " +
             std::to_string(code) + ")";
    }
  };

  static const Category category;
  return category;
}

} // namespace facts::storage
