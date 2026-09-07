#include "analysis/callgraph/CallGraphText.h"

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphOrder.h"
#include <format>

namespace facts::callgraph {
namespace {
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
                                const CoverageReport *coverage, EdgeView view) {
  std::string text;
  for (const auto *root : roots)
    if (std::ranges::find(traversal.nodes, root->id) != traversal.nodes.end())
      text += std::format("root={} usr={}\n", root->name, root->usr);
  for (const auto &value : traversal.edges) {
    const auto *source = detail::findSearchNode(graph, value.edge.source);
    const auto *target = detail::findSearchNode(graph, value.edge.destination);
    const auto semantic =
        view == EdgeView::Semantic
            ? std::format(" semantic-kind={}",
                          semanticKind(target, value.edge.kind))
            : std::string{};
    text += std::format(
        "  depth={} relation={}{} source={} target={} {} implicit={} "
        "location={}:{}:{} cycle={} reused={} external-boundary={} "
        "depth-truncated={}\n",
        value.depth, kindName(value.edge.kind), semantic,
        source ? source->name : "", target ? target->name : "",
        context(value.edge), value.edge.implicit ? "true" : "false",
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
    const auto *source = detail::findSearchNode(graph, value.source);
    const auto *target = detail::findSearchNode(graph, value.target);
    text += std::format("excluded source={} target={} reason={}\n",
                        source ? source->name : "", target ? target->name : "",
                        value.reason);
  }
  for (const auto &value : traversal.excludedNodes) {
    const auto *node = detail::findSearchNode(graph, value.id);
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
