#include "storage/Storage.h"

#include "storage/Sqlite.h"

#include <array>
#include <set>
#include <utility>

namespace facts::storage {
namespace {
using Query = std::pair<std::string_view, std::string_view>;

std::expected<std::set<FileId>, std::error_code>
relevantFiles(Database &database, std::span<const FileId> selected) {
  std::set<FileId> result(selected.begin(), selected.end());
  constexpr std::array<Query, 6> queries{
      {{"symbol", "((id >> 32) & 4294967295)"},
       {"definition", "file_id"},
       {"relation_site", "file_id"},
       {"include_dependency", "src_file_id"},
       {"include_dependency", "dst_file_id"},
       {"callgraph_unresolved_site", "file_id"}}};
  for (const auto &[table, column] : queries) {
    bool present = false;
    try {
      for (const auto value : database.query(
               "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1",
               [](const Row &) { return true; }, table)) {
        present = value;
      }
      if (!present)
        continue;
      for (const auto file : database.query(
               "SELECT DISTINCT " + std::string(column) + " FROM " +
                   std::string(table) + " WHERE " + std::string(column) + ">0",
               [](const Row &row) { return row.get<FileId>(0); })) {
        result.insert(file);
      }
    } catch (const QueryError &error) {
      return std::unexpected(error.code());
    }
  }
  return result;
}
} // namespace

std::expected<void, std::error_code>
registerFactProvenance(Database &database, std::span<const FactProvenance> rows,
                       std::span<const FileId> selected) {
  if (sqlite3_get_autocommit(database.nativeHandle()) != 0)
    return std::unexpected(
        std::make_error_code(std::errc::operation_not_permitted));
  auto relevant = relevantFiles(database, selected);
  if (!relevant)
    return std::unexpected(relevant.error());
  std::vector<storage::FactProvenance> filtered;
  for (const auto file : *relevant) {
    const auto row =
        std::ranges::find(rows, file, &storage::FactProvenance::file);
    if (row == rows.end())
      return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    auto existing = database.query(
        "SELECT path,universe_key FROM facts_project_provenance WHERE "
        "file_id=?1",
        [](const Row &value) {
          return std::pair<std::string, std::string>{value.string(0),
                                                     value.string(1)};
        },
        file);
    try {
      for (const auto &value : existing)
        if (value.first != row->path || value.second != row->universe)
          return std::unexpected(
              std::make_error_code(std::errc::invalid_argument));
    } catch (const QueryError &error) {
      return std::unexpected(error.code());
    }
    filtered.push_back(*row);
  }
  return database
      .executeBulk(
          "INSERT OR IGNORE INTO facts_project_provenance(file_id,path,"
          "universe_key) VALUES(?1,?2,?3)",
          filtered,
          [](sqlite3_stmt *statement, const FactProvenance &row) {
            return bindInteger(statement, 1, row.file) &&
                   bindText(statement, 2, row.path) &&
                   bindText(statement, 3, row.universe);
          },
          {.atomic = false})
      .transform([](const BulkResult &) {});
}
} // namespace facts::storage
