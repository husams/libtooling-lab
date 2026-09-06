#include "analysis/callgraph/CallGraphEntryQuery.h"

#include "storage/SqliteDatabase.h"
#include "storage/catalog/Database.h"

#include <utility>

namespace facts::callgraph {

std::expected<EntryRecord, std::string>
loadCallGraphEntry(const std::string &path, SymbolId symbol) {
  return storage::Database::open(path, storage::Database::readOnly)
      .transform_error([&](const auto &error) {
        return "cannot open facts database '" + path + "': " + error.message();
      })
      .and_then([symbol](auto database) {
        return catalog::query(
                   database,
                   "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND "
                   "name='callgraph_entry'",
                   [](const storage::Row &row) { return row.integer(0); })
            .and_then([&database, symbol](const auto &tables)
                          -> std::expected<EntryRecord, std::string> {
              if (!tables.front()) {
                return EntryRecord{};
              }
              return catalog::query(
                         database,
                         "SELECT symbol_id,graph_node_ref FROM "
                         "callgraph_entry WHERE symbol_id=?",
                         [](const storage::Row &row) {
                           return CallGraphEntry{row.get<SymbolId>(0),
                                                 row.get<SymbolId>(1)};
                         },
                         symbol)
                  .and_then([&database, symbol](auto entries)
                                -> std::expected<EntryRecord, std::string> {
                    if (entries.size() > 1) {
                      return std::unexpected(
                          "facts database contains duplicate call graph "
                          "entries");
                    }
                    auto refs = catalog::query(
                        database,
                        "SELECT r.external_symbol_id,s.qualified_name,s.usr,"
                        "r.kind,r.position,r.file_id,r.offset,site.line,"
                        "site.col FROM callgraph_external_reference r JOIN "
                        "relation_site site ON site.source_id=r.source_id AND "
                        "site.destination_id=r.destination_id AND "
                        "site.kind=r.kind AND site.position=r.position AND "
                        "site.file_id=r.file_id AND site.offset=r.offset JOIN "
                        "symbol s ON s.id=r.external_symbol_id WHERE "
                        "r.source_id=? ORDER BY r.kind,r.position,r.file_id,"
                        "r.offset,r.external_symbol_id",
                        [](const storage::Row &row) {
                          return ExternalTarget{
                              row.get<SymbolId>(0), row.string(1),
                              row.string(2),        row.get<RelationKind>(3),
                              row.get<unsigned>(4), row.get<FileId>(5),
                              row.get<unsigned>(6), row.get<unsigned>(7),
                              row.get<unsigned>(8)};
                        },
                        symbol);
                    if (!refs) {
                      return std::unexpected(refs.error());
                    }
                    return EntryRecord{entries.empty()
                                           ? std::nullopt
                                           : std::optional{entries.front()},
                                       std::move(*refs), false};
                  });
            });
      })
      .transform_error([](std::string error) {
        return "cannot read call graph entry: " + std::move(error);
      });
}

} // namespace facts::callgraph
