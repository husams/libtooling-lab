#include "analysis/callgraph/CallGraphCoverage.h"

#include <algorithm>
#include <ranges>

namespace facts::callgraph {
namespace {
const QueryNode *findNode(const QueryGraph &graph, SymbolId id) {
  const auto found = std::ranges::find(graph.nodes, id, &QueryNode::id);
  return found == graph.nodes.end() ? nullptr : &*found;
}

} // namespace

bool isProjectLocal(const CoverageReport &report, const QueryNode &node) {
  const auto *file = findCoverageEvidenceFile(report, node);
  return file && file->projectLocal;
}

bool hasDefinitionEvidence(const QueryNode &node) {
  return node.definition || node.implicit;
}

const CoverageFile *findCoverageEvidenceFile(const CoverageReport &report,
                                             const QueryNode &node) {
  return findCoverageFile(report, node.definitionLocation
                                      ? node.definitionLocation->file
                                      : node.id.file);
}

std::string definitionAvailability(const CoverageReport &report,
                                   const QueryNode &node) {
  if (hasDefinitionEvidence(node))
    return "available";
  return isProjectLocal(report, node) ? "project-missing"
                                      : "external-unavailable";
}

std::string extractionCoverage(const CoverageReport &report,
                               const QueryNode &node) {
  const auto *file = findCoverageEvidenceFile(report, node);
  if (!file || !file->projectLocal)
    return "not-applicable";
  if (!hasDefinitionEvidence(node))
    return "incomplete";
  if (node.implicit)
    return "complete";
  if (coverageFreshness(report, node) == "stale")
    return "stale";
  return file->indexed ? "complete" : "unknown";
}

std::string coverageAction(const CoverageReport &report,
                           const QueryNode &node) {
  const auto state = extractionCoverage(report, node);
  if (state == "incomplete")
    return report.recoveryCandidates.empty()
               ? "locate-definition-tu"
               : "extract-candidate-translation-unit";
  if (state == "stale")
    return "refresh-source";
  return state == "unknown" ? "reconcile-coverage-metadata" : "none";
}

std::string summarizeCoverage(const CoverageReport &report,
                              const QueryGraph &graph,
                              std::span<const SymbolId> nodes) {
  bool project = false, unknown = false, stale = false;
  for (const auto id : nodes)
    if (const auto *node = findNode(graph, id)) {
      const auto state = extractionCoverage(report, *node);
      if (state == "incomplete")
        return state;
      project |= state != "not-applicable";
      unknown |= state == "unknown";
      stale |= state == "stale";
    }
  return !project  ? "not-applicable"
         : stale   ? "stale"
         : unknown ? "unknown"
                   : "complete";
}
} // namespace facts::callgraph
