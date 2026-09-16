#include "storage/astcache/Connection.h"

#include "storage/FileSchemaMigration.h"

namespace facts::storage::astcache::detail {

Result<storage::Database> open(const std::filesystem::path &path, bool writable) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error))
    return std::unexpected("project configuration database not found: " + path.string());
  return storage::Database::open(path.string(), writable ? SQLITE_OPEN_READWRITE
                                                        : SQLITE_OPEN_READONLY)
      .transform_error([&](auto failure) {
        return "cannot open project configuration " + path.string() + ": " + failure.message();
      })
      .and_then([&](storage::Database database) -> Result<storage::Database> {
        // Metadata is optional, and callers may already hold a project DB
        // transaction. Bound contention instead of waiting on our own lock.
        if (sqlite3_busy_timeout(database.nativeHandle(), 100) != SQLITE_OK)
          return std::unexpected(catalog::databaseError(database));
        return requireCurrentFileSchema(database.nativeHandle())
            .and_then([&] { return catalog::execute(database, "PRAGMA foreign_keys=ON"); })
            .transform([&] { return std::move(database); });
      });
}

} // namespace facts::storage::astcache::detail
