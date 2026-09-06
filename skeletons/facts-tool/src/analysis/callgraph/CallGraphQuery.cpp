#include "analysis/callgraph/CallGraphQuery.h"
#include "analysis/callgraph/CallGraphNodes.h"

#include "storage/SqliteDatabase.h"
#include "storage/catalog/Database.h"

#include <algorithm>
#include <ranges>

namespace facts::callgraph {
namespace {
auto loadEdges(storage::Database &database) {
  return catalog::query(
      database,
      "SELECT site.source_id,site.destination_id,site.kind,site.file_id,"
      "site.line,site.col,site.offset,receiver.id,receiver.qualified_name,"
      "site.certainty,"
      "edge.is_implicit "
      "FROM relation_site site LEFT JOIN symbol receiver ON receiver.id="
      "site.receiver_type_id JOIN symbol source ON source.id=site.source_id "
      "JOIN symbol destination ON destination.id=site.destination_id JOIN "
      "relation edge ON edge.source_id=site.source_id AND edge.destination_id="
      "site.destination_id AND edge.kind=site.kind AND edge.position="
      "site.position WHERE "
      "site.kind IN (?1,?2) ORDER BY source.qualified_name,source.usr,"
      "destination.qualified_name,destination.usr,site.kind,site.file_id,"
      "site.offset",
      [](const storage::Row &row) {
        QueryEdge edge{row.get<SymbolId>(0),
                       row.get<SymbolId>(1),
                       row.get<RelationKind>(2),
                       row.get<FileId>(3),
                       static_cast<unsigned>(row.integer(4)),
                       static_cast<unsigned>(row.integer(5)),
                       static_cast<unsigned>(row.integer(6))};
        if (!row.isNull(7))
          edge.receiverId = row.get<SymbolId>(7);
        if (!row.isNull(8))
          edge.receiver = row.string(8);
        if (!row.isNull(9))
          edge.certainty = row.get<ReceiverCertainty>(9);
        edge.implicit = row.get<bool>(10);
        return edge;
      },
      static_cast<int>(RelationKind::Calls),
      static_cast<int>(RelationKind::DispatchCalls));
}

bool validContext(const QueryEdge &edge) {
  if (!edge.certainty)
    return !edge.receiver;
  if (*edge.certainty == ReceiverCertainty::Exact)
    return edge.receiver.has_value();
  return *edge.certainty == ReceiverCertainty::Possible && !edge.receiver;
}
} // namespace

QueryResult loadCallGraph(const std::string &path) {
  return storage::Database::open(path, storage::Database::readOnly)
      .transform_error([&](const auto &error) {
        return "cannot open facts database '" + path + "': " + error.message();
      })
      .and_then([](auto database) -> QueryResult {
        return loadCallGraphNodes(database).and_then([&](auto nodes)
                                                         -> QueryResult {
          return loadEdges(database).and_then([&](auto edges) -> QueryResult {
            if (!std::ranges::all_of(edges, validContext))
              return std::unexpected("invalid relation-site receiver context");
            return QueryGraph{std::move(nodes), std::move(edges)};
          });
        });
      });
}

std::expected<std::vector<const QueryNode *>, std::string>
selectRoots(const QueryGraph &graph, const std::optional<std::string> &function,
            bool all) {
  std::vector<const QueryNode *> roots;
  for (const auto &node : graph.nodes) {
    const bool selected =
        all ? node.definition
            : function && (node.name == *function || node.usr == *function);
    if (selected)
      roots.push_back(&node);
  }
  if (!all && roots.empty())
    return std::unexpected("function selector not found");
  if (!all && roots.size() != 1)
    return std::unexpected("ambiguous function selector");
  return roots;
}

} // namespace facts::callgraph
