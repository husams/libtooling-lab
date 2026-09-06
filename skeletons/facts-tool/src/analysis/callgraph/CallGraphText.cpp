#include "analysis/callgraph/CallGraphText.h"

#include "analysis/callgraph/CallGraphCoverage.h"
#include <format>
#include <ranges>

namespace facts::callgraph {
namespace {
const QueryNode *findNode(const QueryGraph &graph, SymbolId id) {
  const auto found = std::ranges::find(graph.nodes, id, &QueryNode::id);
  return found == graph.nodes.end() ? nullptr : &*found;
}

std::string_view kindName(RelationKind kind) {
  return kind == RelationKind::DispatchCalls ? "DispatchCalls" : "Calls";
}

std::string context(const QueryEdge &edge) {
  if (!edge.certainty)
    return "receiver=- certainty=-";
  const auto certainty =
      *edge.certainty == ReceiverCertainty::Exact ? "exact" : "possible";
  return std::format("receiver={} certainty={}", edge.receiver.value_or("*"),
                     certainty);
}

std::string location(const QueryEdge &edge, const CoverageReport *coverage) {
  if (coverage)
    if (const auto *file = findCoverageFile(*coverage, edge.file))
      return file->path;
  return std::format("<file {}>", edge.file);
}

} // namespace

std::string renderCallGraphText(const QueryGraph &graph,
                                const std::vector<const QueryNode *> &roots,
                                const RenderedGraph &traversal,
                                const CoverageReport *coverage) {
  std::string text;
  for (const auto *root : roots)
    if (std::ranges::find(traversal.nodes, root->id) != traversal.nodes.end())
      text += std::format("root={} usr={}\n", root->name, root->usr);
  for (const auto &value : traversal.edges) {
    const auto *source = findNode(graph, value.edge.source);
    const auto *target = findNode(graph, value.edge.destination);
    text += std::format(
        "  depth={} relation={} source={} target={} {} location={}:{}:{} "
        "cycle={} reused={} external-boundary={} depth-truncated={}\n",
        value.depth, kindName(value.edge.kind), source ? source->name : "",
        target ? target->name : "", context(value.edge),
        location(value.edge, coverage), value.edge.line, value.edge.column,
        value.cycle ? "true" : "false", value.reused ? "true" : "false",
        value.externalBoundary ? "true" : "false",
        value.depthTruncated ? "true" : "false");
    if (coverage && target)
      text += std::format(
          "    target-usr={} definition-availability={} extraction-coverage={} "
          "freshness={} failure=none-recorded action={}\n",
          target->usr, definitionAvailability(*coverage, *target),
          extractionCoverage(*coverage, *target),
          coverageFreshness(*coverage, *target),
          coverageAction(*coverage, *target));
  }
  for (const auto &value : traversal.excluded) {
    const auto *source = findNode(graph, value.source);
    const auto *target = findNode(graph, value.target);
    text += std::format("excluded source={} target={} reason={}\n",
                        source ? source->name : "", target ? target->name : "",
                        value.reason);
  }
  for (const auto &value : traversal.excludedNodes) {
    const auto *node = findNode(graph, value.id);
    text +=
        std::format("excluded-node id={} name={} reason={}\n",
                    value.id.packed(), node ? node->name : "", value.reason);
  }
  text += std::format("complete={} truncated={}",
                      traversal.truncated == 0 ? "true" : "false",
                      traversal.truncated);
  if (coverage)
    text += " extraction-coverage=" +
            summarizeCoverage(*coverage, graph, traversal.nodes);
  else
    text += " extraction-coverage=unknown";
  text += std::format(" scope={} excluded-nodes={} excluded-edges={} reason={}",
                      scopeName(traversal.scope.calls),
                      traversal.excludedNodes.size(), traversal.excluded.size(),
                      traversal.reason.empty() ? "none" : traversal.reason);
  for (const auto &item : traversal.frontier)
    text += std::format(" frontier={}:{}", item.id.packed(), item.reason);
  return text + '\n';
}
} // namespace facts::callgraph
