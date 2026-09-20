#include "storage/CloneRefresh.h"
#include "storage/SqliteDatabase.h"
#include <algorithm>
#include <array>
#include <set>

namespace facts::storage {
namespace {
std::expected<bool, std::error_code>
moved(Database &database, const FactProvenance &row) {
  try {
    for (const auto &prior : database.query(
        "SELECT path,universe_key FROM facts_project_provenance WHERE file_id=?1",
        [](const Row &value) { return std::pair{value.string(0), value.string(1)}; },
        row.file)) {
      if (prior.second != row.universe || (prior.first != row.path &&
          !std::ranges::contains(row.aliases, prior.first)))
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
      return prior.first != row.path;
    }
    return false;
  } catch (const QueryError &error) { return std::unexpected(error.code()); }
}

std::expected<void, std::error_code>
clearFiles(Database &database, const std::vector<const FactProvenance *> &rows) {
  // The optional receiver FK does not cascade; retain other files' call sites
  // while dropping their obsolete receiver identity before deleting symbols.
  constexpr std::array statements{
      "UPDATE relation_site SET receiver_type_id=NULL "
      "WHERE ((receiver_type_id >> 32) & 4294967295)=?1",
      "DELETE FROM relation_site WHERE file_id=?1",
      "DELETE FROM definition WHERE file_id=?1",
      "DELETE FROM include_dependency WHERE src_file_id=?1",
      "DELETE FROM callgraph_unresolved_site WHERE file_id=?1",
      "DELETE FROM callgraph_pointer_call_site WHERE file_id=?1",
      "DELETE FROM expression_occurrence WHERE file_id=?1",
      "DELETE FROM source_region WHERE file_id=?1",
      "DELETE FROM symbol WHERE ((id >> 32) & 4294967295)=?1"};
  for (const auto *statement : statements) {
    auto result = database.executeBulk(statement, rows,
        [](sqlite3_stmt *query, const FactProvenance *row) {
          return bindInteger(query, 1, row->file);
        }, {.atomic = false});
    if (!result) return std::unexpected(result.error());
  }
  return database.executeBulk(
      "UPDATE facts_project_provenance SET path=?2 WHERE file_id=?1", rows,
      [](sqlite3_stmt *query, const FactProvenance *row) {
        return bindInteger(query, 1, row->file) && bindText(query, 2, row->path);
      }, {.atomic = false}).transform([](const BulkResult &) {});
}
}

std::expected<void, std::error_code>
refreshCloneFiles(Database &database, std::span<const FactProvenance> rows,
                   std::span<const FileId> selected) {
  if (sqlite3_get_autocommit(database.nativeHandle()) != 0)
    return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
  std::set<FileId> requested(selected.begin(), selected.end());
  std::vector<const FactProvenance *> changed;
  for (const auto &row : rows) {
    if (!requested.erase(row.file)) continue;
    if (row.file == builtinFileId)
      return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    if (row.aliases.empty()) continue;
    auto result = moved(database, row);
    if (!result) return std::unexpected(result.error());
    if (*result) changed.push_back(&row);
  }
  if (!requested.empty())
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  return changed.empty() ? std::expected<void, std::error_code>{}
                         : clearFiles(database, changed);
}
}
