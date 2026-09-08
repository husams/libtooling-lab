#include "commands/FactPairValidationInternal.h"

#include "storage/Sqlite.h"

#include <array>
#include <utility>

namespace facts::commands::detail {

std::expected<int, std::string>
factsSchemaVersion(storage::Database &database) {
  auto rows =
      database.query("PRAGMA user_version", [](const storage::Row &row) {
        return static_cast<int>(row.integer(0));
      });
  try {
    for (auto version : rows) {
      // Version 13 adds opt-in expression/source evidence tables.
      if (version > 13) {
        return std::unexpected(
            "incompatible-symbol-universe: unsupported facts schema version");
      }
      return version;
    }
  } catch (const storage::QueryError &error) {
    return std::unexpected(error.what());
  }
  return 0;
}

std::expected<bool, std::string> factsTableExists(storage::Database &database,
                                                  std::string_view name) {
  auto rows = database.query(
      "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?",
      [](const storage::Row &) { return true; }, name);
  try {
    for (auto present : rows) {
      return present;
    }
  } catch (const storage::QueryError &error) {
    return std::unexpected(error.what());
  }
  return false;
}

std::expected<std::set<FileId>, std::string>
usedFactFiles(storage::Database &database) {
  std::set<FileId> result;
  const std::array<std::pair<const char *, const char *>, 12> tables{
      {{"symbol", "((id >> 32) & 4294967295)"},
       {"definition", "file_id"},
       {"relation_site", "file_id"},
       {"include_dependency", "src_file_id"},
       {"include_dependency", "dst_file_id"},
       {"callgraph_unresolved_site", "file_id"},
       {"callgraph_external_reference", "((source_id >> 32) & 4294967295)"},
       {"callgraph_external_reference",
        "((destination_id >> 32) & 4294967295)"},
       {"callgraph_entry", "((symbol_id >> 32) & 4294967295)"},
       {"matched_symbol_index", "file_id"},
       {"expression_occurrence", "file_id"},
       {"source_region", "file_id"}}};
  for (const auto &[table, column] : tables) {
    auto present = factsTableExists(database, table);
    if (!present) {
      return std::unexpected(present.error());
    }
    if (!*present) {
      continue;
    }
    auto rows = database.query(
        "SELECT DISTINCT " + std::string(column) + " FROM " + table +
            " WHERE " + std::string(column) + ">0",
        [](const storage::Row &row) { return row.get<FileId>(0); });
    try {
      for (auto file : rows) {
        result.insert(file);
      }
    } catch (const storage::QueryError &error) {
      return std::unexpected(error.what());
    }
  }
  return result;
}

} // namespace facts::commands::detail
