#include "analysis/callgraph/CallGraphSearch.h"

#include "analysis/callgraph/CallGraphOrder.h"
#include "analysis/callgraph/CallGraphText.h"

#include <map>
#include <set>

namespace facts::callgraph {
namespace {
class CallerSearch {
public:
  CallerSearch(const QueryGraph &graph, std::optional<int> maxDepth,
               const CoverageReport *coverage)
      : graph_(graph), maxDepth_(maxDepth), coverage_(coverage) {
    for (const auto &edge : graph.edges)
      incoming_[edge.destination].push_back(&edge);
    for (auto &[_, edges] : incoming_)
      std::ranges::sort(edges, [&](const auto *left, const auto *right) {
        return detail::edgeLess(graph_, left, right, true);
      });
  }

  RenderedGraph run(const std::vector<const QueryNode *> &roots) {
    for (const auto *root : roots) {
      detail::recordNode(result_.nodes, root->id);
      walk(*root, 0, {});
    }
    result_.text = renderCallGraphText(graph_, roots, result_, coverage_);
    return std::move(result_);
  }

private:
  void walk(const QueryNode &target, int depth, std::set<SymbolId> path) {
    path.insert(target.id);
    expanded_.insert(target.id);
    for (const auto *edge : incoming(target.id)) {
      const auto *source = detail::findSearchNode(graph_, edge->source);
      if (!source)
        continue;
      const auto cycle = path.contains(source->id);
      const auto reused = !cycle && expanded_.contains(source->id);
      const auto boundary = detail::boundaries(*source, coverage_);
      auto childPath = path;
      childPath.insert(source->id);
      const auto more =
          std::ranges::any_of(incoming(source->id), [&](const auto *next) {
            return !childPath.contains(next->source);
          });
      const auto capped = maxDepth_ && depth + 1 >= *maxDepth_ && more &&
                          !boundary.first && !boundary.second;
      result_.truncated += capped;
      detail::recordNode(result_.nodes, source->id);
      detail::recordEdge(result_.edges, *edge, depth + 1, cycle, reused, capped,
                         boundary);
      if (!cycle && !reused && !capped && !boundary.first && !boundary.second)
        walk(*source, depth + 1, path);
    }
  }

  const std::vector<const QueryEdge *> &incoming(SymbolId id) const {
    static const std::vector<const QueryEdge *> empty;
    const auto found = incoming_.find(id);
    return found == incoming_.end() ? empty : found->second;
  }

  const QueryGraph &graph_;
  std::optional<int> maxDepth_;
  const CoverageReport *coverage_;
  std::set<SymbolId> expanded_;
  std::map<SymbolId, std::vector<const QueryEdge *>> incoming_;
  RenderedGraph result_;
};
} // namespace

RenderedGraph searchCallers(const QueryGraph &graph,
                            const std::vector<const QueryNode *> &roots,
                            std::optional<int> maxDepth,
                            const CoverageReport *coverage) {
  return CallerSearch{graph, maxDepth, coverage}.run(roots);
}

} // namespace facts::callgraph
