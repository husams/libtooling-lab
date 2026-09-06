#include "analysis/callgraph/CallGraphJson.h"
#include "analysis/callgraph/CallGraphJsonDetail.h"
#include "analysis/callgraph/CallGraphJsonRequest.h"

#include <llvm/Support/raw_ostream.h>
#include <ranges>

namespace facts::callgraph {

std::string renderCallGraphJson(const QueryGraph &graph,
                                const std::vector<const QueryNode *> &roots,
                                const RenderedGraph &traversal,
                                const CoverageReport *coverage) {
  llvm::json::Array rootValues;
  for (const auto *root : roots)
    if (std::ranges::find(traversal.nodes, root->id) != traversal.nodes.end())
      rootValues.push_back(
          llvm::json::Object{{"id", detail::stableId(root->id)},
                             {"name", root->name},
                             {"usr", root->usr}});
  llvm::json::Array nodeValues;
  for (const auto id : traversal.nodes)
    if (const auto *node = detail::findNode(graph, id))
      nodeValues.push_back(detail::nodeJson(graph, *node, coverage));
  llvm::json::Array edgeValues;
  for (const auto &edge : traversal.edges)
    edgeValues.push_back(detail::edgeJson(graph, edge, coverage));
  llvm::json::Array candidates;
  if (coverage)
    for (const auto &path : coverage->recoveryCandidates)
      candidates.push_back(path);
  const auto state = coverage
                         ? summarizeCoverage(*coverage, graph, traversal.nodes)
                         : std::string{"unknown"};
  llvm::json::Object output{
      {"schema", "facts-tool.call-graph.v1"},
      {"complete", traversal.truncated == 0},
      {"truncated", traversal.truncated},
      {"query", queryJson(traversal)},
      {"coverage",
       llvm::json::Object{{"traversal_complete", traversal.truncated == 0}}},
      {"truncation", truncationJson(graph, traversal)},
      {"excluded_scope", excludedScopeJson(graph, traversal)},
      {"recovery", llvm::json::Object{}},
      {"errors", llvm::json::Array{}},
      {"traversal",
       llvm::json::Object{{"complete", traversal.truncated == 0},
                          {"depth_truncated", traversal.reason == "max_depth"
                                                  ? traversal.truncated
                                                  : 0}}},
      {"pair",
       llvm::json::Object{{"state", coverage ? "validated" : "unavailable"}}},
      {"extraction_coverage",
       llvm::json::Object{{"state", state},
                          {"failure", nullptr},
                          {"recovery_candidates", std::move(candidates)}}},
      {"roots", std::move(rootValues)},
      {"nodes", std::move(nodeValues)},
      {"edges", std::move(edgeValues)}};
  std::string text;
  llvm::raw_string_ostream stream(text);
  stream << llvm::json::Value(std::move(output)) << '\n';
  return text;
}
} // namespace facts::callgraph
