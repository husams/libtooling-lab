#include "analysis/callgraph/CallGraphJsonRequest.h"

#include "analysis/callgraph/CallGraphJsonDetail.h"

#include <set>

namespace facts::callgraph {
namespace {
llvm::json::Value number(const std::optional<std::uint64_t> &value) {
  return value ? llvm::json::Value(static_cast<std::int64_t>(*value))
               : llvm::json::Value(nullptr);
}

llvm::json::Value number(const std::optional<int> &value) {
  return value ? llvm::json::Value(*value) : llvm::json::Value(nullptr);
}

llvm::json::Object scopeObject(const ScopeSelection &scope) {
  llvm::json::Array components;
  for (const auto &name : scope.requestedComponents)
    components.push_back(name);
  return llvm::json::Object{{"calls", std::string(scopeName(scope.calls))},
                            {"components", std::move(components)}};
}
} // namespace

llvm::json::Object queryJson(const RenderedGraph &traversal) {
  llvm::json::Object limits{
      {"max_depth", number(traversal.limits.depth)},
      {"max_nodes", number(traversal.limits.nodes)},
      {"max_edges", number(traversal.limits.edges)},
      {"time_limit_ms", number(traversal.limits.time.transform([](auto value) {
         return static_cast<std::uint64_t>(value.count());
       }))}};
  return llvm::json::Object{{"scope", scopeObject(traversal.scope)},
                            {"limits", std::move(limits)}};
}

llvm::json::Object truncationJson(const QueryGraph &graph,
                                  const RenderedGraph &traversal) {
  llvm::json::Array frontier;
  for (const auto &item : traversal.frontier) {
    const auto *node = detail::findNode(graph, item.id);
    frontier.push_back(llvm::json::Object{{"id", detail::stableId(item.id)},
                                          {"name", node ? node->name : ""},
                                          {"reason", item.reason}});
  }
  llvm::json::Object result{{"reached", !traversal.reason.empty()},
                            {"frontier", std::move(frontier)}};
  result["reason"] = traversal.reason.empty()
                         ? llvm::json::Value(nullptr)
                         : llvm::json::Value(traversal.reason);
  return result;
}

llvm::json::Object excludedScopeJson(const QueryGraph &graph,
                                     const RenderedGraph &traversal) {
  llvm::json::Array nodes, edges;
  std::set<SymbolId> unique;
  for (const auto &item : traversal.excludedNodes) {
    const auto *node = detail::findNode(graph, item.id);
    unique.insert(item.id);
    nodes.push_back(llvm::json::Object{{"id", detail::stableId(item.id)},
                                       {"name", node ? node->name : ""},
                                       {"reason", item.reason}});
  }
  for (const auto &item : traversal.excluded) {
    unique.insert(item.target);
    edges.push_back(
        llvm::json::Object{{"source_id", detail::stableId(item.source)},
                           {"target_id", detail::stableId(item.target)},
                           {"reason", item.reason}});
  }
  return llvm::json::Object{
      {"active", traversal.scope.active()},
      {"filters", scopeObject(traversal.scope)},
      {"nodes", std::move(nodes)},
      {"edges", std::move(edges)},
      {"observed_node_count", static_cast<std::int64_t>(unique.size())},
      {"observed_edge_count",
       static_cast<std::int64_t>(traversal.excluded.size())}};
}

} // namespace facts::callgraph
