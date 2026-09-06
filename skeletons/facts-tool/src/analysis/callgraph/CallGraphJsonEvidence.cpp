#include "analysis/callgraph/CallGraphJsonDetail.h"

#include <format>
#include <ranges>

namespace facts::callgraph::detail {

const QueryNode *findNode(const QueryGraph &graph, SymbolId id) {
  const auto found = std::ranges::find(graph.nodes, id, &QueryNode::id);
  return found == graph.nodes.end() ? nullptr : &*found;
}

std::string stableId(SymbolId id) {
  return std::format("{}:{}", id.file, id.index);
}

llvm::json::Object nodeJson(const QueryGraph &graph, const QueryNode &node,
                            const CoverageReport *coverage) {
  const auto outgoing = std::ranges::any_of(
      graph.edges, [&](const auto &edge) { return edge.source == node.id; });
  const auto *sourceFile =
      coverage ? findCoverageFile(*coverage, node.id.file) : nullptr;
  const auto *file =
      coverage ? findCoverageEvidenceFile(*coverage, node) : nullptr;
  const auto availability = coverage ? definitionAvailability(*coverage, node)
                            : node.definition ? "available"
                            : node.external   ? "external-unavailable"
                                              : "unknown";
  llvm::json::Object source{
      {"file_id", node.id.file}, {"line", node.line}, {"column", node.column}};
  source["path"] = sourceFile ? llvm::json::Value(sourceFile->path)
                              : llvm::json::Value(nullptr);
  source["project_local"] = sourceFile
                                ? llvm::json::Value(sourceFile->projectLocal)
                                : llvm::json::Value(nullptr);
  llvm::json::Object evidence{
      {"state", coverage ? extractionCoverage(*coverage, node) : "unknown"},
      {"freshness", coverage ? coverageFreshness(*coverage, node) : "unknown"},
      {"action",
       coverage ? coverageAction(*coverage, node) : "supply-project-conf"}};
  evidence["catalog_indexed"] =
      file ? llvm::json::Value(file->indexed) : llvm::json::Value(nullptr);
  evidence["indexed_at"] = file && !file->indexedAt.empty()
                               ? llvm::json::Value(file->indexedAt)
                               : llvm::json::Value(nullptr);
  evidence["catalog_mtime"] = file && file->mtime
                                  ? llvm::json::Value(*file->mtime)
                                  : llvm::json::Value(nullptr);
  evidence["failure"] = nullptr;
  return llvm::json::Object{
      {"id", stableId(node.id)},
      {"name", node.name},
      {"usr", node.usr},
      {"definition_availability", availability},
      {"source", std::move(source)},
      {"definition", definitionJson(node, coverage)},
      {"facts", llvm::json::Object{{"definition", node.definition},
                                   {"outgoing_calls", outgoing}}},
      {"coverage", std::move(evidence)}};
}

llvm::json::Object edgeJson(const QueryGraph &graph, const TraversedEdge &value,
                            const CoverageReport *coverage) {
  const auto *source = findNode(graph, value.edge.source);
  const auto *target = findNode(graph, value.edge.destination);
  const auto *file =
      coverage ? findCoverageFile(*coverage, value.edge.file) : nullptr;
  const auto relation = value.edge.kind == RelationKind::DispatchCalls
                            ? "DispatchCalls"
                            : "Calls";
  llvm::json::Object location{{"file_id", value.edge.file},
                              {"line", value.edge.line},
                              {"column", value.edge.column},
                              {"offset", value.edge.offset}};
  location["path"] =
      file ? llvm::json::Value(file->path) : llvm::json::Value(nullptr);
  llvm::json::Object edge{{"depth", value.depth},
                          {"relation", relation},
                          {"source_id", stableId(value.edge.source)},
                          {"target_id", stableId(value.edge.destination)},
                          {"source_usr", source ? source->usr : ""},
                          {"target_usr", target ? target->usr : ""},
                          {"location", std::move(location)},
                          {"cycle", value.cycle},
                          {"reused", value.reused},
                          {"external_boundary", value.externalBoundary},
                          {"definition_boundary", value.definitionBoundary},
                          {"depth_truncated", value.depthTruncated}};
  edge["receiver"] = value.edge.receiver
                         ? llvm::json::Value(*value.edge.receiver)
                         : llvm::json::Value(nullptr);
  edge["certainty"] =
      value.edge.certainty
          ? llvm::json::Value(*value.edge.certainty == ReceiverCertainty::Exact
                                  ? "exact"
                                  : "possible")
          : llvm::json::Value(nullptr);
  return edge;
}
} // namespace facts::callgraph::detail
