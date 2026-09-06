#include "analysis/callgraph/CallGraphNodes.h"

namespace facts::callgraph {
catalog::Result<std::vector<QueryNode>>
loadCallGraphNodes(catalog::Database &database) {
  return catalog::query(
      database,
      "SELECT s.id,s.qualified_name,s.usr,s.is_definition,s.is_external,"
      "s.line,s.col,d.file_id,d.offset,d.size,s.kind,s.is_implicit FROM symbol "
      "s LEFT JOIN "
      "definition d ON d.symbol_id=s.id WHERE s.node=1 OR "
      "s.kind IN (13,17,18,19,23,24,25) ORDER BY s.qualified_name,s.usr,s.id",
      [](const storage::Row &row) {
        std::optional<QueryDefinition> definition;
        if (!row.isNull(7))
          definition = QueryDefinition{row.get<FileId>(7),
                                       static_cast<unsigned>(row.integer(8)),
                                       static_cast<unsigned>(row.integer(9))};
        return QueryNode{row.get<SymbolId>(0),
                         row.string(1),
                         row.string(2),
                         row.get<bool>(3),
                         row.get<bool>(4),
                         static_cast<unsigned>(row.integer(5)),
                         static_cast<unsigned>(row.integer(6)),
                         definition,
                         row.integer(10),
                         row.get<bool>(11)};
      });
}
} // namespace facts::callgraph
