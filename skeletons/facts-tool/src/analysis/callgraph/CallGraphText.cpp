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

bool outgoing(const QueryGraph &graph, const QueryNode &node) {
  return std::ranges::any_of(
      graph.edges, [&](const auto &edge) { return edge.source == node.id; });
}
} // namespace

std::string renderCallGraphText(const QueryGraph &graph,
                                const std::vector<const QueryNode *> &roots,
                                const RenderedGraph &traversal,
                                const CoverageReport *coverage) {
  std::string text;
  for (const auto *root : roots)
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
          coverageAction(*coverage, *target, outgoing(graph, *target)));
  }
  text += std::format("complete={} truncated={}",
                      traversal.truncated == 0 ? "true" : "false",
                      traversal.truncated);
  if (coverage)
    text += " extraction-coverage=" +
            summarizeCoverage(*coverage, graph, traversal.nodes);
  return text + '\n';
}
} // namespace facts::callgraph
