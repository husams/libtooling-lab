#include "analysis/callgraph/CallGraphNodes.h"

namespace facts::callgraph {

catalog::Result<std::vector<QueryPointerCall>>
loadCallGraphPointerCalls(catalog::Database &database,
                         std::optional<SymbolId> source) {
  return catalog::query(
             database,
             "SELECT EXISTS(SELECT 1 FROM sqlite_master WHERE type='table' "
             "AND name='callgraph_pointer_call_site')",
             [](const storage::Row &row) { return row.get<bool>(0); })
      .and_then([&](const auto &tables)
                    -> catalog::Result<std::vector<QueryPointerCall>> {
        if (!tables.front())
          return std::vector<QueryPointerCall>{};
        const std::string sql =
            "SELECT p.source_id,p.target_id,p.file_id,p.offset,p.line,p.col,"
            "p.signature,p.expression,s.qualified_name,s.usr "
            "FROM callgraph_pointer_call_site p LEFT JOIN symbol s ON "
            "s.id=p.target_id ";
        const std::string order = " ORDER BY p.source_id,p.file_id,p.offset";
        const auto map = [](const storage::Row &row) {
          const auto target = row.isNull(1)
                                  ? std::nullopt
                                  : std::optional{row.get<SymbolId>(1)};
          return QueryPointerCall{
              PointerCallSite{row.get<SymbolId>(0), target, row.get<FileId>(2),
                              {row.get<unsigned>(4), row.get<unsigned>(5),
                               row.get<unsigned>(3)},
                              row.string(6), row.string(7)},
              target ? std::optional{PointerCallTarget{
                           *target, row.string(8), row.string(9)}}
                     : std::nullopt};
        };
        return source
                   ? catalog::query(database, sql + "WHERE p.source_id=?" + order,
                                     map, *source)
                   : catalog::query(database, sql + order, map);
      });
}
} // namespace facts::callgraph
