#include "apis/v2/JobServices.h"
#include "analysis/callgraph/CallGraphQuery.h"
#include "analysis/callgraph/CallGraphSearch.h"
#include "storage/catalog/Database.h"
#include <algorithm>
#include <map>
#include <set>
#include <tuple>

namespace facts::apis::v2::jobs {
namespace {
struct Scope { std::int64_t id = 0; bool repository = false; };
struct Graph {
  callgraph::QueryGraph value;
  std::map<FileId, Scope> scopes;
  Json diagnostics = Json::array();
  bool complete = true;
  std::string identity(const callgraph::QueryNode &node) const {
    auto scope = scopes.find(node.definitionLocation ? node.definitionLocation->file : node.id.file);
    const auto value = scope == scopes.end() ? Scope{} : scope->second;
    return index::symbolIdentity(node.usr.empty() ? node.name : node.usr,
                                  value.id, value.repository);
  }
  std::string identity(SymbolId id) const {
    const auto node = std::ranges::find(value.nodes, id, &callgraph::QueryNode::id);
    return node == value.nodes.end() ? std::to_string(id.file) + ":" + std::to_string(id.index)
                                    : identity(*node);
  }
};
Result<std::map<FileId, Scope>> scopes(const domain::Context &context) {
  return catalog::open(context.configuration.database.string(), false)
      .and_then([](catalog::Database database) {
        return catalog::query(database,
            "SELECT f.id,coalesce(c.repository_id,c.id),c.repository_id IS NOT NULL "
            "FROM file f JOIN directory d ON d.id=f.directory_id JOIN component c ON c.id=d.component_id",
            [](const storage::Row &row) { return std::pair{static_cast<FileId>(row.integer(0)),
                Scope{row.integer(1), row.integer(2) != 0}}; });
      }).transform([](const auto &rows) { return std::map<FileId, Scope>{rows.begin(), rows.end()}; })
      .transform_error(failed);
}
Result<std::set<FileId>> invalidatedFiles(const domain::Context &context) {
  return catalog::open(context.configuration.database.string(), false)
      .and_then([](catalog::Database database) -> catalog::Result<std::vector<FileId>> {
        auto tables = catalog::query(database,
            "SELECT 1 FROM sqlite_master WHERE type='table' AND name='api_index_invalidated_file'",
            [](const storage::Row &row) { return row.integer(0); });
        if (!tables) return std::unexpected(tables.error());
        if (tables->empty()) return std::vector<FileId>{};
        return catalog::query(database, "SELECT file_id FROM api_index_invalidated_file",
            [](const storage::Row &row) { return static_cast<FileId>(row.integer(0)); });
      }).transform([](const auto &rows) { return std::set<FileId>{rows.begin(), rows.end()}; })
      .transform_error(failed);
}
Result<Graph> loadGraph(const domain::Context &context, const runtime::Request &request) {
  return scopes(context).and_then([&](auto identityScopes) -> Result<Graph> {
    auto paths = domain::factSources(context);
    if (!paths) return std::unexpected(paths.error());
    auto invalidated = invalidatedFiles(context);
    if (!invalidated) return std::unexpected(invalidated.error());
    Graph result{{}, std::move(identityScopes)};
    std::map<SymbolId, callgraph::QueryNode> nodes;
    std::uint32_t nextIndex = 1;
    unsigned missingSources = 0;
    for (const auto &path : *paths) {
      auto allowed = checkpoint(request);
      if (!allowed) return std::unexpected(allowed.error());
      std::error_code error;
      if (!std::filesystem::exists(path, error)) {
        result.complete = false;
        ++missingSources;
        continue;
      }
      auto graph = callgraph::loadCallGraph(path.string()).transform_error(failed);
      if (!graph) return std::unexpected(graph.error());
      // Symbol allocators belong to each facts database. Remap its local IDs
      // before combining databases: the same numeric ID can name another USR.
      std::map<SymbolId, SymbolId> local;
      std::set<SymbolId> invalidSources;
      for (auto &node : graph->nodes) {
        if (nextIndex == 0) return std::unexpected(failed("Call graph exceeds the supported node count"));
        const auto original = node.id;
        if (invalidated->contains(node.definitionLocation ? node.definitionLocation->file : node.id.file)) {
          invalidSources.insert(original);
          node.definition = false;
          node.bodyEvidence = false;
          node.implicit = false;
        }
        node.id.index = nextIndex++;
        local[original] = node.id;
        nodes.emplace(node.id, std::move(node));
      }
      for (auto &edge : graph->edges) {
        if (invalidated->contains(edge.file) || invalidSources.contains(edge.source)) continue;
        if (!local.contains(edge.source) || !local.contains(edge.destination)) continue;
        edge.source = local.at(edge.source);
        edge.destination = local.at(edge.destination);
        if (edge.receiverId && local.contains(*edge.receiverId)) edge.receiverId = local.at(*edge.receiverId);
        result.value.edges.push_back(std::move(edge));
      }
      for (auto &call : graph->pointerCalls) {
        if (local.contains(call.site.source)) call.site.source = local.at(call.site.source);
        result.value.pointerCalls.push_back(std::move(call));
      }
    }
    if (missingSources || !invalidated->empty()) {
      result.complete = false;
      result.diagnostics.push_back({{"severity", "warning"},
          {"message", "Graph coverage is incomplete: " + std::to_string(missingSources) +
              " facts sources unavailable, " + std::to_string(invalidated->size()) + " files invalidated"},
          {"file", ""}, {"line", 0}, {"column", 0}});
    }
    // A USR in one repository is one function across declarations and TUs.
    // Canonicalize it before traversal so calls reach definitions from another TU.
    std::map<std::string, SymbolId> canonical;
    for (const auto &[id, node] : nodes) {
      const auto key = result.identity(node);
      auto found = canonical.find(key);
      if (found == canonical.end() || (!nodes.at(found->second).definition && node.definition))
        canonical[key] = id;
    }
    for (const auto &[id, node] : nodes) {
      auto &combined = nodes.at(canonical.at(result.identity(node)));
      combined.unresolved = std::max(combined.unresolved, node.unresolved);
      combined.pointerCalls = std::max(combined.pointerCalls, node.pointerCalls);
      combined.bodyEvidence = combined.bodyEvidence || node.bodyEvidence;
      combined.external = combined.external && node.external;
    }
    std::map<SymbolId, SymbolId> remap;
    for (auto &[id, node] : nodes) {
      const auto target = canonical.at(result.identity(node));
      remap[id] = target;
      if (target == id) result.value.nodes.push_back(std::move(node));
    }
    std::set<std::tuple<SymbolId, SymbolId, int, FileId, unsigned, unsigned>> seen;
    std::erase_if(result.value.edges, [&](auto &edge) {
      if (!remap.contains(edge.source) || !remap.contains(edge.destination)) return true;
      edge.source = remap.at(edge.source);
      edge.destination = remap.at(edge.destination);
      if (edge.receiverId && remap.contains(*edge.receiverId)) edge.receiverId = remap.at(*edge.receiverId);
      return !seen.emplace(edge.source, edge.destination, static_cast<int>(edge.kind),
                            edge.file, edge.offset, edge.position).second;
    });
    for (auto &call : result.value.pointerCalls)
      if (remap.contains(call.site.source)) call.site.source = remap.at(call.site.source);
    return result;
  });
}
Result<const callgraph::QueryNode *> selectNode(const Graph &graph, const index::Symbol &symbol) {
  const auto found = std::ranges::find_if(graph.value.nodes,
      [&](const auto &node) { return graph.identity(node) == symbol.symbolId; });
  if (found == graph.value.nodes.end()) return std::unexpected(domain::Error{
      404, "function_not_found", "Selected symbol has no stored call-graph function"});
  return &*found;
}
Json encodeNode(const Graph &graph, const callgraph::QueryNode &node) {
  return {{"symbol_id", graph.identity(node)}, {"qualified_name", node.name},
          {"usr", node.usr}, {"definition", node.definition}, {"external", node.external},
          {"file_id", std::to_string(node.id.file)}, {"line", node.line}, {"column", node.column},
          {"unresolved_calls", node.unresolved}, {"pointer_calls", node.pointerCalls}};
}
Json encodeEdge(const Graph &graph, const callgraph::QueryEdge &edge) {
  return {{"source", graph.identity(edge.source)}, {"target", graph.identity(edge.destination)},
          {"kind", edge.kind == RelationKind::DispatchCalls ? "dispatch_call" : "call"},
          {"file_id", std::to_string(edge.file)}, {"line", edge.line}, {"column", edge.column},
          {"offset", edge.offset}, {"implicit", edge.implicit}};
}
Json encode(const Graph &graph, const callgraph::TraversalResult &traversal,
            const std::vector<callgraph::QueryPath> &paths) {
  Json result{{"coverage", graph.complete ? "complete" : "partial"}, {"truncated", traversal.truncated != 0},
      {"truncation_reason", traversal.reason.empty() ? Json(nullptr) : Json(traversal.reason)},
      {"nodes", Json::array()}, {"edges", Json::array()}, {"paths", Json::array()},
      {"frontier", Json::array()}, {"diagnostics", graph.diagnostics}};
  const std::set<SymbolId> reached(traversal.nodes.begin(), traversal.nodes.end());
  for (const auto &node : graph.value.nodes) if (reached.contains(node.id)) {
    result["nodes"].push_back(encodeNode(graph, node));
    if (node.unresolved || node.pointerCalls || !node.definition) result["coverage"] = "partial";
  }
  for (const auto &edge : traversal.edges) {
    auto value = encodeEdge(graph, edge.edge);
    value["depth"] = edge.depth;
    value["cycle"] = edge.cycle;
    value["external_boundary"] = edge.externalBoundary;
    value["definition_boundary"] = edge.definitionBoundary;
    result["edges"].push_back(std::move(value));
  }
  for (const auto &node : traversal.frontier)
    result["frontier"].push_back({{"symbol_id", graph.identity(node.id)}, {"reason", node.reason}});
  for (const auto &path : paths) {
    Json nodes = Json::array();
    for (const auto &node : path.nodes) nodes.push_back(graph.identity(node));
    result["paths"].push_back({{"nodes", std::move(nodes)}});
  }
  if (traversal.truncated) result["coverage"] = "partial";
  result["node_count"] = result["nodes"].size();
  result["edge_count"] = result["edges"].size();
  return result;
}
}
Result<Json> callGraph(const domain::Context &context, const runtime::Request &request) {
  return resolveSymbol(context, request.options.at("root")).and_then([&](const auto &rootSymbol) {
    return loadGraph(context, request).and_then([&](const Graph &graph) -> Result<Json> {
      auto root = selectNode(graph, rootSymbol);
      if (!root) return std::unexpected(root.error());
      callgraph::TraversalRequest controls;
      controls.cancelled = request.cancelled;
      controls.limits.depth = request.options.value("max_depth", 32);
      controls.limits.nodes = request.options.value("max_nodes", 10000);
      controls.limits.edges = request.options.value("max_edges", 50000);
      controls.limits.time = std::chrono::milliseconds(request.options.value("time_limit_ms", 30000));
      callgraph::TraversalResult traversal;
      std::vector<callgraph::QueryPath> paths;
      if (request.options.contains("target")) {
        auto symbol = resolveSymbol(context, request.options.at("target"));
        if (!symbol) return std::unexpected(symbol.error());
        auto target = selectNode(graph, *symbol);
        if (!target) return std::unexpected(target.error());
        const auto mode = request.options.value("path_mode", "shortest") == "shortest"
            ? callgraph::PathMode::Shortest : callgraph::PathMode::AllSimple;
        auto search = callgraph::searchPathsWithRequest(graph.value, **root, **target, mode, controls);
        traversal = std::move(search.traversal);
        paths = std::move(search.paths);
      } else if (request.options.value("direction", "callees") == "callers")
        traversal = callgraph::searchCallersWithRequest(graph.value, {*root}, controls);
      else traversal = callgraph::traverseCallGraph(graph.value, {*root}, controls);
      if (traversal.reason == "cancelled") return std::unexpected(domain::Error{
          409, "cancelled", "Call-graph traversal was cancelled"});
      return encode(graph, traversal, paths);
    });
  });
}
}
