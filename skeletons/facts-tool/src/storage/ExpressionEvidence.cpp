#include "storage/Storage.h"

#include "storage/SqliteDatabase.h"
#include "storage/SqliteQuery.h"

namespace facts {

std::expected<void, std::error_code> Storage::addExpressionOccurrences(
    std::span<const ExpressionOccurrence> occurrences) {
  return database_
      .executeBulk(
          "INSERT OR IGNORE INTO expression_occurrence("
          "identity,owner_id,target_id,file_id,line,col,offset,size,"
          "source_sha256,expression_kind,access,freshness,unavailable_reason) "
          "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13)",
          occurrences,
          [](sqlite3_stmt *statement, const ExpressionOccurrence &occurrence) {
            return storage::bindParameters(
                statement, occurrence.identity, occurrence.owner,
                occurrence.target, occurrence.file, occurrence.line,
                occurrence.column, occurrence.offset, occurrence.size,
                occurrence.sourceSha256, occurrence.expressionKind,
                occurrence.access, occurrence.freshness,
                occurrence.unavailableReason);
          },
          {.atomic = false})
      .transform([](const storage::BulkResult &) {});
}

std::expected<void, std::error_code>
Storage::addSourceRegions(std::span<const SourceRegion> regions) {
  return database_
      .executeBulk(
          "INSERT OR IGNORE INTO source_region("
          "identity,symbol_id,file_id,line,col,offset,size,source_sha256,"
          "symbol_kind,freshness,unavailable_reason) "
          "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11)",
          regions,
          [](sqlite3_stmt *statement, const SourceRegion &region) {
            return storage::bindParameters(
                statement, region.identity, region.symbol, region.file,
                region.line, region.column, region.offset, region.size,
                region.sourceSha256, region.symbolKind, region.freshness,
                region.unavailableReason);
          },
          {.atomic = false})
      .transform([](const storage::BulkResult &) {});
}

} // namespace facts
