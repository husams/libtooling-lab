#include "analysis/callgraph/CallGraphTraversal.h"

#include "analysis/callgraph/CallGraphContext.h"
#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphText.h"

#include <ranges>
#include <set>

namespace facts::callgraph {
namespace {

const QueryNode *findNode(const QueryGraph &graph, SymbolId id) {
  const auto found = std::ranges::find(graph.nodes, id, &QueryNode::id);
  return found == graph.nodes.end() ? nullptr : &*found;
}

class Traversal {
public:
  Traversal(const QueryGraph &graph, std::optional<int> maxDepth,
            const CoverageReport *coverage)
      : graph_(graph), maxDepth_(maxDepth), coverage_(coverage) {}

  RenderedGraph run(const std::vector<const QueryNode *> &roots) {
    for (const auto *root : roots) {
      recordNode(root->id);
      walk(*root, {root->id, {}, {}}, 0, {});
    }
    return {{}, truncated_, std::move(nodes_), std::move(edges_)};
  }

private:
  void recordNode(SymbolId id) {
    if (std::ranges::find(nodes_, id) == nodes_.end())
      nodes_.push_back(id);
  }

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
      const bool external = coverage_ ? !target->definition &&
                                            !isProjectLocal(*coverage_, *target)
                                      : target->external || !target->definition;
      const bool definitionBoundary = coverage_ && !target->definition &&
                                      isProjectLocal(*coverage_, *target);
      recordNode(target->id);
      edges_.push_back({edge, depth + 1, cycle, reused, external,
                        definitionBoundary, capped});
      if (!cycle && !reused && !external && !definitionBoundary && !capped)
        walk(*target, child, depth + 1, path);
    }
  }

  const QueryGraph &graph_;
  std::optional<int> maxDepth_;
  const CoverageReport *coverage_;
  std::set<QueryContext> expanded_;
  unsigned truncated_ = 0;
  std::vector<SymbolId> nodes_;
  std::vector<TraversedEdge> edges_;
};

} // namespace

RenderedGraph renderCallGraph(const QueryGraph &graph,
                              const std::vector<const QueryNode *> &roots,
                              std::optional<int> maxDepth,
                              const CoverageReport *coverage, EdgeView view) {
  auto traversal = Traversal{graph, maxDepth, coverage}.run(roots);
  traversal.text = renderCallGraphText(graph, roots, traversal, coverage, view);
  return traversal;
}

} // namespace facts::callgraph
