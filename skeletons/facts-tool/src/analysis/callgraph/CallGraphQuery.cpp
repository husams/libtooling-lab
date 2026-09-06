#include "analysis/callgraph/CallGraphQuery.h"
#include "analysis/callgraph/CallGraphNodes.h"
#include "analysis/callgraph/CallGraphSelection.h"

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
      "site.line,site.col,site.offset,receiver.qualified_name,site.certainty,"
      "site.position "
      "FROM relation_site site LEFT JOIN symbol receiver ON receiver.id="
      "site.receiver_type_id JOIN symbol source ON source.id=site.source_id "
      "JOIN symbol destination ON destination.id=site.destination_id WHERE "
      "site.kind IN (?1,?2) ORDER BY source.qualified_name,source.usr,"
      "destination.qualified_name,destination.usr,site.kind,site.position,"
      "site.file_id,site.offset",
      [](const storage::Row &row) {
        QueryEdge edge{row.get<SymbolId>(0),
                       row.get<SymbolId>(1),
                       row.get<RelationKind>(2),
                       row.get<FileId>(3),
                       static_cast<unsigned>(row.integer(4)),
                       static_cast<unsigned>(row.integer(5)),
                       static_cast<unsigned>(row.integer(6))};
        if (!row.isNull(7))
          edge.receiver = row.string(7);
        if (!row.isNull(8))
          edge.certainty = row.get<ReceiverCertainty>(8);
        edge.position = static_cast<unsigned>(row.integer(9));
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
  if (!all)
    return selectOne(graph, *function, "root").transform([](const auto *root) {
      return std::vector{root};
    });
  std::vector<const QueryNode *> roots;
  for (const auto &node : graph.nodes) {
    if (node.definition)
      roots.push_back(&node);
  }
  return roots;
}

} // namespace facts::callgraph
