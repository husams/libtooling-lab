#include "analysis/callgraph/CallGraphTraversal.h"

#include "analysis/callgraph/CallGraphContext.h"

#include <algorithm>
#include <format>
#include <ranges>
#include <set>

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

class Traversal {
public:
  Traversal(const QueryGraph &graph, std::optional<int> maxDepth)
      : graph_(graph), maxDepth_(maxDepth) {}

  RenderedGraph run(const std::vector<const QueryNode *> &roots) {
    for (const auto *root : roots) {
      text_ += std::format("root={} usr={}\n", root->name, root->usr);
      walk(*root, {root->id, {}, {}}, 0, {});
    }
    text_ += std::format("complete={} truncated={}\n",
                         truncated_ == 0 ? "true" : "false", truncated_);
    return {std::move(text_), truncated_};
  }

private:
  void walk(const QueryNode &source, const QueryContext &current, int depth,
            std::set<QueryContext> path) {
    path.insert(current);
    expanded_.insert(current);
    for (const auto &edge : graph_.edges) {
      if (edge.source != source.id || !matchesContext(edge, current))
        continue;
      const auto *target = findNode(graph_, edge.destination);
      if (!target)
        continue;
      const auto child = descendContext(edge, current);
      const bool cycle = path.contains(child);
      const bool reused = !cycle && expanded_.contains(child);
      const bool capped =
          maxDepth_ && depth + 1 >= *maxDepth_ &&
          std::ranges::any_of(graph_.edges, [&](const auto &next) {
            return next.source == target->id && matchesContext(next, child);
          });
      if (capped)
        ++truncated_;
      text_ += std::format(
          "  depth={} relation={} source={} target={} {} location=<file {}>:"
          "{}:{} cycle={} reused={} external-boundary={} depth-truncated={}\n",
          depth + 1, kindName(edge.kind), source.name, target->name,
          context(edge), edge.file, edge.line, edge.column,
          cycle ? "true" : "false", reused ? "true" : "false",
          target->external ? "true" : "false", capped ? "true" : "false");
      if (!cycle && !reused && !target->external && !capped)
        walk(*target, child, depth + 1, path);
    }
  }

  const QueryGraph &graph_;
  std::optional<int> maxDepth_;
  std::set<QueryContext> expanded_;
  std::string text_;
  unsigned truncated_ = 0;
};

} // namespace

RenderedGraph renderCallGraph(const QueryGraph &graph,
                              const std::vector<const QueryNode *> &roots,
                              std::optional<int> maxDepth) {
  return Traversal{graph, maxDepth}.run(roots);
}

} // namespace facts::callgraph
