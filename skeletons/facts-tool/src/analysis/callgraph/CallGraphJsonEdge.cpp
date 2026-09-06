#include "analysis/callgraph/CallGraphJsonDetail.h"

#include "analysis/callgraph/CallGraphSemantics.h"

namespace facts::callgraph::detail {

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
  llvm::json::Object edge{
      {"depth", value.depth},
      {"relation", relation},
      {"relation_kind", relation},
      {"semantic_kind", std::string{semanticKind(target, value.edge.kind)}},
      {"source_id", stableId(value.edge.source)},
      {"target_id", stableId(value.edge.destination)},
      {"source_usr", source ? source->usr : ""},
      {"target_usr", target ? target->usr : ""},
      {"location", std::move(location)},
      {"implicit", value.edge.implicit},
      {"cycle", value.cycle},
      {"reused", value.reused},
      {"external_boundary", value.externalBoundary},
      {"definition_boundary", value.definitionBoundary},
      {"depth_truncated", value.depthTruncated}};
  edge["receiver"] = value.edge.receiver
                         ? llvm::json::Value(*value.edge.receiver)
                         : llvm::json::Value(nullptr);
  edge["receiver_type_id"] =
      value.edge.receiverId
          ? llvm::json::Value(stableId(*value.edge.receiverId))
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
