#include "analysis/callgraph/CallGraphJson.h"
#include "analysis/callgraph/CallGraphJsonDetail.h"

#include <llvm/Support/raw_ostream.h>

namespace facts::callgraph {

std::string renderCallGraphJson(const QueryGraph &graph,
                                const std::vector<const QueryNode *> &roots,
                                const RenderedGraph &traversal,
                                const CoverageReport *coverage, EdgeView view) {
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
    edgeValues.push_back(detail::edgeJson(graph, edge, coverage));
  llvm::json::Array candidates;
  if (coverage)
    for (const auto &path : coverage->recoveryCandidates)
      candidates.push_back(path);
  const auto state = coverage
                         ? summarizeCoverage(*coverage, graph, traversal.nodes)
                         : std::string{"unknown"};
  llvm::json::Object extractionCoverage{
      {"state", state},
      {"failure", nullptr},
      {"recovery_candidates", std::move(candidates)}};
  extractionCoverage["unsupported_semantics"] =
      llvm::json::Object{{"state", "not-persisted"},
                         {"action", "inspect-extraction-diagnostics"},
                         {"sites", llvm::json::Array{}}};
  llvm::json::Object output{
      {"schema", "facts-tool.call-graph.v1"},
      {"edge_view", std::string{edgeViewName(view)}},
      {"complete", traversal.truncated == 0},
      {"truncated", traversal.truncated},
      {"traversal",
       llvm::json::Object{{"complete", traversal.truncated == 0},
                          {"depth_truncated", traversal.truncated}}},
      {"pair",
       llvm::json::Object{{"state", coverage ? "validated" : "unavailable"}}},
      {"extraction_coverage", std::move(extractionCoverage)},
      {"roots", std::move(rootValues)},
      {"nodes", std::move(nodeValues)},
      {"edges", std::move(edgeValues)}};
  std::string text;
  llvm::raw_string_ostream stream(text);
  stream << llvm::json::Value(std::move(output)) << '\n';
  return text;
}
} // namespace facts::callgraph
