#include "storage/DependencyDatabase.h"

#include "storage/Sqlite.h"
#include "storage/SqliteDatabase.h"

#include <sqlite3.h>

namespace facts {
namespace {

std::expected<void, std::error_code>
deleteVisitedSources(storage::Database &database,
                     std::span<const FileId> visitedSources) {
  return database
      .executeBulk("DELETE FROM include_dependency WHERE src_file_id=?1",
                   visitedSources,
                   [](sqlite3_stmt *statement, FileId source) {
                     return storage::bindInteger(statement, 1, source);
                   })
      .transform([](const storage::BulkResult &) {});
}

std::expected<void, std::error_code>
insertDependencies(storage::Database &database,
                   std::span<const DependencyEdge> edges) {
  return database
      .executeBulk("INSERT INTO include_dependency(src_file_id,dst_file_id) "
                   "VALUES(?1,?2)",
                   edges,
                   [](sqlite3_stmt *statement, const DependencyEdge &edge) {
                     return storage::bindInteger(statement, 1, edge.source) &&
                            storage::bindInteger(statement, 2,
                                                 edge.destination);
                   })
      .transform([](const storage::BulkResult &) {});
}

std::expected<void, std::error_code>
replace(storage::Database &database, std::span<const FileId> visitedSources,
        std::span<const DependencyEdge> edges) {
  return database.write().and_then([&](storage::Transaction transaction) {
    return deleteVisitedSources(database, visitedSources)
        .and_then([&] { return insertDependencies(database, edges); })
        .and_then([&] { return transaction.commit(); });
  });
}

} // namespace

std::expected<void, std::error_code>
replaceDependencies(const std::string &databasePath,
                    std::span<const FileId> visitedSources,
                    std::span<const DependencyEdge> edges) {
  return storage::Database::open(databasePath, storage::Database::readWrite)
      .and_then([&](storage::Database database) {
        return database.executeScript("PRAGMA foreign_keys=ON").and_then([&] {
          return replace(database, visitedSources, edges);
        });
      });
}

} // namespace facts
