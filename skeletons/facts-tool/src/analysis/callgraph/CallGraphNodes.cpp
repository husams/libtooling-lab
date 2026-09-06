#include "analysis/callgraph/CallGraphNodes.h"

#include <string>

namespace facts::callgraph {
catalog::Result<std::vector<QueryNode>>
loadCallGraphNodes(catalog::Database &database) {
  auto map = [](const storage::Row &row) {
    std::optional<QueryDefinition> definition;
    if (!row.isNull(8))
      definition = QueryDefinition{row.get<FileId>(8),
                                   static_cast<unsigned>(row.integer(9)),
                                   static_cast<unsigned>(row.integer(10))};
    return QueryNode{row.get<SymbolId>(0), row.string(1), row.string(2),
                     row.get<bool>(3), row.get<bool>(4),
                     static_cast<unsigned>(row.integer(5)),
                     static_cast<unsigned>(row.integer(6)), definition,
                     static_cast<unsigned>(row.integer(7))};
  };
  auto load = [&](std::string unresolved) {
    return catalog::query(
        database,
        "SELECT s.id,s.qualified_name,s.usr,s.is_definition,s.is_external,"
        "s.line,s.col," + unresolved +
            ",d.file_id,d.offset,d.size FROM symbol s LEFT JOIN definition d "
            "ON d.symbol_id=s.id WHERE s.node=1 OR "
            "s.kind IN (13,17,18,19,23,24,25) ORDER BY s.qualified_name,s.usr,s.id",
        map);
  };
  return catalog::query(
             database,
             "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND "
             "name='callgraph_unresolved_site'",
             [](const storage::Row &row) { return row.integer(0); })
      .and_then([&](const auto &tables) {
        return load(tables.front() ?
                        "(SELECT COUNT(*) FROM callgraph_unresolved_site u "
                        "WHERE u.source_id=s.id)"
                                    : "0");
      });
}
} // namespace facts::callgraph
