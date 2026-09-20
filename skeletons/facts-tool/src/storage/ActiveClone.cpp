#include "storage/ActiveClone.h"
#include "storage/SqliteDatabase.h"

namespace facts::storage {
namespace {
std::expected<bool, std::error_code>
needsSwitch(Database &database, std::int64_t repository, std::int64_t clone) {
  return prepare(database.nativeHandle(),
      "SELECT r.active_clone_id IS NOT ?2 FROM repository r JOIN clone c "
      "ON c.repository_id=r.id WHERE r.id=?1 AND c.id=?2")
      .and_then([&](Statement query) -> std::expected<bool, std::error_code> {
        if (!bindParameters(query.get(), repository, clone))
          return std::unexpected(sqliteError(database.nativeHandle()));
        const auto status = sqlite3_step(query.get());
        if (status == SQLITE_DONE)
          return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
        if (status != SQLITE_ROW)
          return std::unexpected(sqliteError(database.nativeHandle()));
        return sqlite3_column_int(query.get(), 0) != 0;
      });
}

template <class... Binds>
std::expected<void, std::error_code>
writeSelection(Database &database, std::string_view sql, const Binds &...binds) {
  return prepare(database.nativeHandle(), sql)
      .and_then([&](Statement query) -> std::expected<void, std::error_code> {
        if (!bindParameters(query.get(), binds...) ||
            sqlite3_step(query.get()) != SQLITE_DONE)
          return std::unexpected(sqliteError(database.nativeHandle()));
        return {};
      });
}
}

std::expected<void, std::error_code>
activateRegisteredClone(Database &database, std::int64_t repository,
                         std::int64_t clone) {
  auto connection = detail::Connection(database.nativeHandle(), [](sqlite3 *) {});
  return detail::BulkScope::start(std::move(connection), true)
      .and_then([&](detail::BulkScope transaction) {
    return needsSwitch(database, repository, clone)
        .and_then([&](bool changed) -> std::expected<void, std::error_code> {
          if (!changed) return {};
          return writeSelection(database,
              "UPDATE repository SET active_clone_id=?2 WHERE id=?1", repository, clone)
              .and_then([&] {
                return writeSelection(database,
                    "UPDATE file SET indexed=0 WHERE directory_id IN "
                    "(SELECT d.id FROM directory d JOIN component c ON "
                    "c.id=d.component_id WHERE c.repository_id=?1)", repository);
              });
        }).and_then([&] { return transaction.complete(); });
  });
}
}
