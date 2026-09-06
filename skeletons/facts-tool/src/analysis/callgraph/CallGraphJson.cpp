#include "analysis/callgraph/CallGraphJson.h"
#include "analysis/callgraph/CallGraphJsonDetail.h"
#include "analysis/callgraph/CallGraphJsonQuery.h"
#include "analysis/callgraph/CallGraphJsonRecovery.h"

#include <llvm/Support/raw_ostream.h>

#include <ranges>

namespace facts::callgraph {

std::string renderCallGraphJson(
    const QueryGraph &graph, const std::vector<const QueryNode *> &roots,
    const RenderedGraph &traversal, const CoverageReport *coverage,
    EdgeView view, QueryMode mode, std::optional<PathMode> pathMode,
    const QueryNode *target, const std::vector<QueryPath> &paths,
    std::string_view result, const RecoveryReport *recovery) {
  llvm::json::Array rootValues;
  for (const auto *root : roots)
    rootValues.push_back(llvm::json::Object{{"id", detail::stableId(root->id)},
                                            {"name", root->name},
                                            {"usr", root->usr}});
  llvm::json::Array nodeValues;
  for (const auto id : traversal.nodes)
    if (const auto *node = detail::findNode(graph, id))
      nodeValues.push_back(detail::nodeJson(graph, *node, coverage));
  llvm::json::Array edgeValues;
  for (const auto &edge : traversal.edges)
    edgeValues.push_back(detail::edgeJson(graph, edge, coverage, view));
  const auto unresolved = std::ranges::fold_left(
      traversal.nodes, 0U, [&](unsigned count, SymbolId id) {
        const auto *node = detail::findNode(graph, id);
        return count + (node ? node->unresolved : 0U);
      });
  auto recoverySections =
      json::recoverySections(graph, coverage, traversal.nodes);
  llvm::json::Object output{
      {"schema", "facts-tool.call-graph.v1"},
      {"schema_version", 1},
      {"edge_view", std::string{edgeViewName(view)}},
      {"complete", traversal.truncated == 0},
      {"truncated", traversal.truncated},
      {"unresolved", unresolved},
      {"traversal",
       llvm::json::Object{{"complete", traversal.truncated == 0},
                          {"depth_truncated", traversal.truncated}}},
      {"pair",
       llvm::json::Object{{"state", coverage ? "validated" : "unavailable"}}},
      {"extraction_coverage", std::move(recoverySections.extractionCoverage)},
      {"coverage",
       llvm::json::Object{{"missing_definitions",
                           std::move(recoverySections.missingDefinitions)},
                          {"unresolved_targets",
                           std::move(recoverySections.unresolvedTargets)}}},
      {"roots", std::move(rootValues)},
      {"nodes", std::move(nodeValues)},
      {"edges", std::move(edgeValues)}};
  detail::addQueryJson(output, mode, pathMode, target, paths, result);
  if (recovery)
    output["recovery"] = json::recoveryObject(*recovery);
  output["errors"] = json::recoveryErrors(recovery);
  std::string text;
  llvm::raw_string_ostream stream(text);
  stream << llvm::json::Value(std::move(output)) << '\n';
  return text;
}
} // namespace facts::callgraph
