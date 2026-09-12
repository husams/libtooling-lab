#include "commands/CallGraphInvalidation.h"

#include "commands/ConfigurationSupport.h"
#include "commands/DatabasePaths.h"
#include "commands/FactPairValidation.h"
#include "storage/FactStore.h"
#include "storage/FileIndexState.h"
#include "storage/SqliteDatabase.h"
#include "storage/catalog/Database.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <vector>

namespace facts::commands {
std::expected<bool, std::string>
invalidateConfiguredCallGraphEntries(const config::Resolved &resolved,
                                     const std::string &explicitFacts,
                                     const std::vector<std::string> &sources) {
  auto paths = configuredCallGraphFactPaths(resolved, explicitFacts, sources);
  if (!paths)
    return std::unexpected(paths.error());
  bool invalidatedAny = false;
  for (const auto &path : *paths) {
    auto validPaths = validateDatabasePaths(path, resolved.database.string());
    if (!validPaths)
      return std::unexpected(validPaths.error());
    if (!std::filesystem::exists(path) ||
        !std::filesystem::is_regular_file(resolved.database))
      continue;
    auto invalidated = invalidateCallGraphEntriesBeforeMutation(
        resolved.database.string(), path);
    if (!invalidated)
      return std::unexpected(invalidated.error());
    invalidatedAny = invalidatedAny || *invalidated;
  }
  return invalidatedAny;
}

std::expected<void, std::string> resetIndexState(storage::Database &database) {
  return fileIndexStateColumnsPresent(database.nativeHandle())
      .transform_error([](std::error_code error) {
        return "cannot reset index state: " + error.message();
      })
      .and_then([&](bool present) -> std::expected<void, std::string> {
        if (!present)
          return {};
        return database
            .execute("UPDATE file SET indexed=0,indexed_at=NULL,mtime=NULL,"
                     "facts_db=NULL,git_commit=NULL")
            .transform_error([](std::error_code error) {
              return "cannot reset index state: " + error.message();
            });
      });
}

std::expected<void, std::string>
resetIndexState(const std::string &configuration) {
  if (!std::filesystem::is_regular_file(configuration))
    return {};
  auto database =
      storage::Database::open(configuration, storage::Database::readWrite);
  if (!database)
    return std::unexpected("cannot reset index state: " +
                           database.error().message());
  return resetIndexState(*database);
}

std::expected<void, std::string>
resetIndexStateForIds(storage::Database &database,
                      std::span<const FileId> ids) {
  if (ids.empty())
    return {};
  return fileIndexStateColumnsPresent(database.nativeHandle())
      .transform_error([](std::error_code error) {
        return "cannot reset index state: " + error.message();
      })
      .and_then([&](bool present) -> std::expected<void, std::string> {
        if (!present)
          return {};
        return database
            .executeBulk("UPDATE file SET indexed=0,indexed_at=NULL,"
                        "mtime=NULL,facts_db=NULL,git_commit=NULL "
                        "WHERE id=?1",
                        ids,
                        [](sqlite3_stmt *statement, const FileId &id) {
                          return storage::bindParameters(statement, id);
                        })
            .transform_error([](std::error_code error) {
              return "cannot reset index state: " + error.message();
            })
            .transform([](const storage::BulkResult &) {});
      });
}

std::expected<bool, std::string>
invalidateCallGraphEntriesBeforeMutation(const std::string &configuration,
                                         const std::string &facts) {
  if (facts.empty() || !std::filesystem::exists(facts))
    return false;
  return validateFactPairForRead(facts, configuration)
      .and_then([&] { return catalog::open(configuration, false); })
      .and_then([](auto database) {
        return catalog::query(
            database, "SELECT id FROM file ORDER BY id",
            [](const storage::Row &row) { return row.get<FileId>(0); });
      })
      .and_then([&facts](auto ids) -> std::expected<bool, std::string> {
        try {
          FactStore store(facts);
          return store.invalidateCallGraphEntries(ids)
              .transform_error([](std::error_code error) {
                return "cannot invalidate call graph entries: " +
                       error.message();
              })
              .transform([] { return true; });
        } catch (const std::exception &error) {
          return std::unexpected("cannot invalidate call graph entries: " +
                                 std::string(error.what()));
        }
      });
}

} // namespace facts::commands
