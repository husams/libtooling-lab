#include "storage/FileDatabase.h"

#include "storage/FileSchema.h"

#include <sqlite3.h>

#include <functional>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace facts {
namespace {

using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

std::error_code sqliteError(sqlite3 *database) {
  return {sqlite3_extended_errcode(database), std::generic_category()};
}

std::expected<void, std::error_code> execute(sqlite3 *database,
                                             const char *sql) {
  if (sqlite3_exec(database, sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
    return std::unexpected(sqliteError(database));
  }
  return {};
}

std::expected<Statement, std::error_code> prepare(sqlite3 *database,
                                                  const char *sql) {
  sqlite3_stmt *raw = nullptr;
  if (sqlite3_prepare_v2(database, sql, -1, &raw, nullptr) != SQLITE_OK) {
    return std::unexpected(sqliteError(database));
  }
  return Statement(raw, sqlite3_finalize);
}

std::expected<void, std::error_code>
bindText(sqlite3 *database, sqlite3_stmt *statement, std::string_view value) {
  if (sqlite3_bind_text(statement, 1, value.data(),
                        static_cast<int>(value.size()),
                        SQLITE_TRANSIENT) != SQLITE_OK) {
    return std::unexpected(sqliteError(database));
  }
  return {};
}

std::expected<FileId, std::error_code> readId(sqlite3 *database,
                                              sqlite3_stmt *statement) {
  if (sqlite3_step(statement) != SQLITE_ROW) {
    return std::unexpected(
        std::make_error_code(std::errc::no_such_file_or_directory));
  }
  const auto raw = sqlite3_column_int64(statement, 0);
  if (raw < firstPhysicalFileId || raw > std::numeric_limits<FileId>::max()) {
    return std::unexpected(std::make_error_code(std::errc::value_too_large));
  }
  return static_cast<FileId>(raw);
}

std::expected<void, std::error_code> upsertFile(sqlite3 *database,
                                                std::string_view identity) {
  constexpr auto sql = "INSERT INTO file(path) VALUES(?1) "
                       "ON CONFLICT(path) DO NOTHING";
  return prepare(database, sql).and_then([&](Statement statement) {
    return bindText(database, statement.get(), identity).and_then([&] {
      if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        return std::expected<void, std::error_code>{
            std::unexpected(sqliteError(database))};
      }
      return std::expected<void, std::error_code>{};
    });
  });
}

std::expected<FileId, std::error_code> selectId(sqlite3 *database,
                                                std::string_view identity) {
  return prepare(database, "SELECT id FROM file WHERE path=?1")
      .and_then([&](Statement statement) {
        return bindText(database, statement.get(), identity).and_then([&] {
          return readId(database, statement.get());
        });
      });
}

template <std::ranges::input_range Results>
auto collect(Results &&results)
    -> std::expected<void,
                     typename std::ranges::range_value_t<Results>::error_type> {
  using Result = std::ranges::range_value_t<Results>;
  using Error = typename Result::error_type;
  for (auto result : results) {
    if (!result) {
      return std::unexpected<Error>(result.error());
    }
  }
  return std::expected<void, Error>{};
}

template <typename Work>
auto inImmediateTransaction(sqlite3 *database, Work work)
    -> std::invoke_result_t<Work> {
  auto started = execute(database, "BEGIN IMMEDIATE");
  if (!started) {
    return std::unexpected(started.error());
  }

  auto result = std::invoke(std::move(work));
  if (!result) {
    execute(database, "ROLLBACK");
    return result;
  }

  auto committed = execute(database, "COMMIT");
  if (!committed) {
    execute(database, "ROLLBACK");
    return std::unexpected(committed.error());
  }
  return result;
}

} // namespace

struct FileDatabase::Connection {
  explicit Connection(const std::string &path) {
    constexpr int flags =
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2(path.c_str(), &handle, flags, nullptr) != SQLITE_OK) {
      const std::string message =
          handle ? sqlite3_errmsg(handle) : "cannot allocate SQLite handle";
      if (handle) {
        sqlite3_close(handle);
        handle = nullptr;
      }
      throw std::runtime_error(message);
    }
    sqlite3_busy_timeout(handle, 10000);
    if (sqlite3_exec(handle, fileSchemaSql, nullptr, nullptr, nullptr) !=
        SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(handle));
    }
  }

  ~Connection() {
    if (handle) {
      sqlite3_close(handle);
    }
  }

  sqlite3 *handle = nullptr;
};

FileDatabase::FileDatabase(const std::string &path)
    : connection_(std::make_unique<Connection>(path)) {}

FileDatabase::~FileDatabase() = default;

std::expected<void, std::error_code>
FileDatabase::addBulk(std::span<const std::string> identities) {
  return inImmediateTransaction(connection_->handle, [&] {
    auto records =
        identities | std::views::transform([&](const auto &identity) {
          return upsertFile(connection_->handle, identity);
        });
    return collect(records);
  });
}

std::expected<FileId, std::error_code>
FileDatabase::getId(std::string_view identity) {
  return selectId(connection_->handle, identity);
}

} // namespace facts
