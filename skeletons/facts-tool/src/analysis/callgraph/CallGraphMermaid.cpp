#include "analysis/callgraph/CallGraphMermaid.h"
#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphOrder.h"
#include <format>
#include <set>

namespace facts::callgraph {
namespace {
std::string escape(std::string_view value) {
  std::string result;
  for (const unsigned char c : value)
    result += c < 32 || c == 127 || std::string_view("\"&<>#;|\\").contains(c)
                  ? std::format("#{};", static_cast<unsigned>(c))
                  : std::string(1, c);
  return result;
}

std::string nodeId(SymbolId id, std::string_view scope) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const unsigned char c : scope)
    hash = (hash ^ c) * 1099511628211ULL;
  return std::format("n{:x}_{:x}", hash, id.packed());
}
} // namespace

std::string
renderCallGraphMermaid(const QueryGraph &graph, const RenderedGraph &traversal,
                       std::string_view metadata, std::string_view facts,
                       const CoverageReport *coverage, EdgeView view,
                       bool initial, bool recoveryFailed) {
  std::string text = "%% facts-tool call graph\n%% stage=";
  text += initial ? "initial incomplete=true recovery=pending\n"
                  : "final extraction-freshness=unknown-unless-validated\n";
  text += "%% " + std::string(metadata) + "flowchart TD\n";
  const auto state =
      initial          ? "Initial partial graph; recovery pending"
      : recoveryFailed ? "Partial graph; recovery failed"
      : traversal.truncated
          ? "Partial graph; traversal truncated"
          : "Stored traversal complete; extraction freshness separate";
  text += std::format("  graph_status[\"{}\"]\n", state);
  for (const auto id : traversal.nodes) {
    const auto *node = detail::findSearchNode(graph, id);
    if (!node)
      continue;
    auto label = node->name;
    if (!hasDefinitionEvidence(*node))
      label += coverage && isProjectLocal(*coverage, *node)
                   ? " [unresolved project definition]"
                   : " [external boundary]";
    if (node->unresolved)
      label += " [unresolved calls=" + std::to_string(node->unresolved) + "]";
    text += std::format("  {}[\"{}\"]\n", nodeId(id, facts), escape(label));
  }
  std::set<std::string> emitted;
  for (const auto &item : traversal.edges) {
    const auto &edge = item.edge;
    auto label =
        edge.kind == RelationKind::DispatchCalls ? "DispatchCalls" : "Calls";
    std::string description = label;
    if (view == EdgeView::Semantic)
      description +=
          " / " +
          std::string(semanticKind(
              detail::findSearchNode(graph, edge.destination), edge.kind));
    const auto *file =
        coverage ? findCoverageFile(*coverage, edge.file) : nullptr;
    description +=
        std::format(" @ {}:{}:{} offset={}",
                    file ? file->path : std::format("file {}", edge.file),
                    edge.line, edge.column, edge.offset);
    const auto line =
        std::format("  {} -->|\"{}\"| {}\n", nodeId(edge.source, facts),
                    escape(description), nodeId(edge.destination, facts));
    if (emitted.insert(line).second)
      text += line;
  }
  return text;
}
} // namespace facts::callgraph
