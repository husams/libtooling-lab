#include "commands/analyse/CallGraphRunStore.h"

#include "commands/analyse/CallGraphRunRows.h"
#include "storage/SchemaMigration.h"
#include "storage/SqliteDatabase.h"
#include "storage/catalog/Database.h"

namespace facts::commands {
namespace {
std::string failure(std::string_view detail) {
  return "cannot persist call graph run: " + std::string{detail};
}

std::expected<std::int64_t, std::string>
writeRun(storage::Database &database, const CallGraphRunRecord &record) {
  // The run and every child row land in one BEGIN IMMEDIATE transaction; a
  // rollback on any failure leaves the store exactly as it was.
  auto transaction = database.write();
  if (!transaction)
    return std::unexpected(failure(catalog::databaseError(database)));
  if (auto migrated = storage::migrateSchema(database.nativeHandle());
      !migrated)
    return std::unexpected(failure(catalog::databaseError(database)));
  auto runId = insertCallGraphRun(database, record);
  if (!runId)
    return std::unexpected(failure(runId.error()));
  if (auto rows = insertCallGraphRunRows(database, *runId, record); !rows)
    return std::unexpected(failure(rows.error()));
  if (auto committed = transaction->commit(); !committed)
    return std::unexpected(failure(catalog::databaseError(database)));
  return *runId;
}
} // namespace

std::expected<std::int64_t, std::string>
persistCallGraphRun(const CallGraphRunRecord &record) {
  auto opened = storage::Database::open(
      record.factsPath, storage::Database::readWrite | SQLITE_OPEN_FULLMUTEX);
  if (!opened)
    return std::unexpected(failure(opened.error().message()));
  return writeRun(*opened, record);
}
} // namespace facts::commands
