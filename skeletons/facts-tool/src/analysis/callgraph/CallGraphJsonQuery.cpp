#include "analysis/callgraph/CallGraphJsonQuery.h"

#include <format>

namespace facts::callgraph::detail {
namespace {
std::string edgeId(const QueryEdge &edge) {
  return std::format(
      "{}>{}:{}:{}:{}:{}", stableId(edge.source), stableId(edge.destination),
      static_cast<unsigned>(edge.kind), edge.position, edge.file, edge.offset);
}

llvm::json::Object targetJson(const QueryNode &target) {
  return llvm::json::Object{
      {"id", stableId(target.id)}, {"name", target.name}, {"usr", target.usr}};
}
} // namespace

void addQueryJson(llvm::json::Object &output, QueryMode mode,
                  std::optional<PathMode> pathMode, const QueryNode *target,
                  const std::vector<QueryPath> &paths,
                  std::string_view result) {
  auto query = output.getObject("query") ? std::move(*output.getObject("query"))
                                         : llvm::json::Object{};
  query["mode"] = std::string(queryModeName(mode));
  query["path_mode"] =
      pathMode ? llvm::json::Value(std::string(pathModeName(*pathMode)))
               : llvm::json::Value(nullptr);
  query["target"] = target ? llvm::json::Value(targetJson(*target))
                           : llvm::json::Value(nullptr);
  output["query"] = std::move(query);
  llvm::json::Array pathValues;
  for (const auto &path : paths) {
    llvm::json::Array nodes, edges;
    for (const auto id : path.nodes)
      nodes.push_back(stableId(id));
    for (const auto &edge : path.edges)
      edges.push_back(edgeId(edge));
    pathValues.push_back(llvm::json::Object{{"node_ids", std::move(nodes)},
                                            {"edge_keys", std::move(edges)}});
  }
  output["paths"] = std::move(pathValues);
  output["path_result"] = result.empty()
                              ? llvm::json::Value(nullptr)
                              : llvm::json::Value(std::string(result));
}

} // namespace facts::callgraph::detail
