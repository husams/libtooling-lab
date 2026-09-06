#include "analysis/callgraph/CallGraphTraversalEngine.h"

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphScope.h"

namespace facts::callgraph {

bool TraversalEngine::hasOutgoing(const QueryNode &node,
                                  const QueryContext &context) const {
  for (const auto &edge : graph_.edges)
    if (edge.source == node.id && matchesContext(edge, context))
      if (const auto *target = findNode(edge.destination);
          target && allowed(*target))
        return true;
  return false;
}

void TraversalEngine::walk(const QueryNode &source, const QueryContext &current,
                           int depth, std::set<QueryContext> path) {
  path.insert(current);
  expanded_.insert(current);
  for (const auto &edge : graph_.edges) {
    if (stopRequested(source.id))
      return;
    if (edge.source != source.id || !matchesContext(edge, current))
      continue;
    const auto *target = findNode(edge.destination);
    if (!target)
      continue;
    if (const auto reason = exclusionReason(*target, request_.scope, coverage_);
        !reason.empty()) {
      exclude(edge, reason);
      continue;
    }
    if (!admit(edge, target->id))
      return;
    const auto child = descendContext(edge, current);
    const bool cycle = path.contains(child);
    const bool reused = !cycle && expanded_.contains(child);
    const bool capped = request_.limits.depth &&
                        depth + 1 >= *request_.limits.depth &&
                        hasOutgoing(*target, child);
    if (capped)
      truncate(target->id, "max_depth");
    const bool external =
        coverage_ ? !target->definition && !isProjectLocal(*coverage_, *target)
                  : target->external || !target->definition;
    const bool definition =
        coverage_ && !target->definition && isProjectLocal(*coverage_, *target);
    result_.edges.push_back(
        {edge, depth + 1, cycle, reused, external, definition, capped});
    if (!cycle && !reused && !external && !definition && !capped)
      walk(*target, child, depth + 1, path);
  }
}

RenderedGraph
TraversalEngine::run(const std::vector<const QueryNode *> &roots) {
  result_.scope = request_.scope;
  result_.limits = request_.limits;
  for (const auto *root : roots) {
    if (!allowed(*root)) {
      result_.excludedNodes.push_back(
          {root->id, exclusionReason(*root, request_.scope, coverage_)});
      continue;
    }
    if (stopRequested(root->id) || !admitNode(root->id))
      break;
    walk(*root, {root->id, {}, {}}, 0, {});
  }
  return std::move(result_);
}

} // namespace facts::callgraph
