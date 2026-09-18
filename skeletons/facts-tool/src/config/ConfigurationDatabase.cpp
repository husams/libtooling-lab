#include "config/Configuration.h"
#include "config/ConfigurationLock.h"
#include "storage/FileDatabase.h"
#include "storage/FileSchemaMigration.h"
#include "storage/SqliteDatabase.h"

#include <exception>
#include <filesystem>

namespace facts::config {
namespace {

std::expected<void, std::string> requireReusableDatabase(sqlite3 *database) {
  // An empty database can be initialized. Older interrupted imports could
  // leave only this obsolete table; its contents have no bearing on which
  // repositories or clones the catalog may contain.
  constexpr std::string_view sql =
      "SELECT EXISTS(SELECT 1 FROM sqlite_master "
      "WHERE name NOT GLOB 'sqlite_*' "
      "AND NOT (type='table' AND name='generated_conf_owner'))";
  return storage::prepare(database, sql)
      .transform_error([](auto error) {
        return "cannot inspect project configuration: " + error.message();
      })
      .and_then([&](storage::Statement statement)
                    -> std::expected<void, std::string> {
        if (sqlite3_step(statement.get()) != SQLITE_ROW)
          return std::unexpected("cannot inspect project configuration: " +
                                 storage::sqliteError(database).message());
        return sqlite3_column_int(statement.get(), 0) == 0
                   ? std::expected<void, std::string>{}
                   : requireSupportedFileSchema(database);
      });
}

std::expected<void, std::string>
initializeDatabase(const std::filesystem::path &path) {
  return storage::Database::open(path.string(), storage::Database::readWrite)
      .transform_error([](auto error) {
        return "cannot open project configuration: " + error.message();
      })
      .and_then([](storage::Database database) {
        return requireReusableDatabase(database.nativeHandle());
      })
      .and_then([&]() -> std::expected<void, std::string> {
        try {
          FileDatabase database(path.string());
          return {};
        } catch (const std::exception &error) {
          return std::unexpected(error.what());
        }
      });
}

} // namespace

std::expected<void, std::string> prepareDatabase(const Resolved &resolved) {
  auto target = renderDatabasePath(resolved);
  if (!target)
    return std::unexpected(target.error());
  std::error_code error;
  std::filesystem::create_directories(resolved.database.parent_path(), error);
  if (error)
    return std::unexpected(error.message());
  auto lock = detail::ParentLock::acquire(resolved.database.parent_path());
  if (!lock)
    return std::unexpected(lock.error());
  // Revalidate after locking, then initialize the catalog before another
  // generated-path writer can inspect it. Catalog identity comes from its
  // schema; repository locations belong to the clone table.
  return renderDatabasePath(resolved).and_then([&](const auto &) {
    return initializeDatabase(resolved.database).transform_error(
        [&](const auto &message) {
          return resolved.database.string() + ": " + message;
        });
  });
}

} // namespace facts::config
