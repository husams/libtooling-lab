#include "analysis/callgraph/CallGraphTraversalEngine.h"

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphScope.h"

#include <algorithm>
#include <ranges>
namespace facts::callgraph {

TraversalEngine::TraversalEngine(const QueryGraph &graph,
                                 TraversalRequest request,
                                 const CoverageReport *coverage)
    : graph_(graph), request_(std::move(request)), coverage_(coverage),
      started_(std::chrono::steady_clock::now()) {}

const QueryNode *TraversalEngine::findNode(SymbolId id) const {
  const auto found = std::ranges::find(graph_.nodes, id, &QueryNode::id);
  return found == graph_.nodes.end() ? nullptr : &*found;
}

bool TraversalEngine::allowed(const QueryNode &node) const {
  return included(node, request_.scope, coverage_);
}

void TraversalEngine::truncate(SymbolId id, std::string reason) {
  const auto known =
      std::ranges::any_of(result_.frontier, [&](const auto &item) {
        return item.id == id && item.reason == reason;
      });
  if (!known)
    result_.frontier.push_back({id, reason});
  if (result_.reason.empty() || reason != "max_depth")
    result_.reason = reason;
  result_.truncated = static_cast<unsigned>(result_.frontier.size());
}

bool TraversalEngine::stopRequested(SymbolId frontier) {
  if (result_.reason != "max_depth" && !result_.reason.empty())
    return true;
  if (request_.cancelled && request_.cancelled()) {
    truncate(frontier, "cancelled");
    return true;
  }
  if (request_.limits.time &&
      std::chrono::steady_clock::now() - started_ >= *request_.limits.time) {
    truncate(frontier, "time_limit");
    return true;
  }
  return false;
}

bool TraversalEngine::admitNode(SymbolId id) {
  if (std::ranges::find(result_.nodes, id) != result_.nodes.end())
    return true;
  if (request_.limits.nodes && result_.nodes.size() >= *request_.limits.nodes) {
    truncate(id, "max_nodes");
    return false;
  }
  result_.nodes.push_back(id);
  return true;
}

bool TraversalEngine::admit(const QueryEdge &edge, SymbolId target) {
  const EdgeKey key{edge.source, edge.destination, edge.kind, edge.position};
  const bool newNode =
      std::ranges::find(result_.nodes, target) == result_.nodes.end();
  const bool newEdge = !edgeKeys_.contains(key);
  if (newNode && request_.limits.nodes &&
      result_.nodes.size() >= *request_.limits.nodes) {
    truncate(target, "max_nodes");
    return false;
  }
  if (newEdge && request_.limits.edges &&
      edgeKeys_.size() >= *request_.limits.edges) {
    truncate(target, "max_edges");
    return false;
  }
  if (newNode)
    result_.nodes.push_back(target);
  if (newEdge)
    edgeKeys_.insert(key);
  return true;
}

void TraversalEngine::exclude(const QueryEdge &edge, std::string reason) {
  const EdgeKey key{edge.source, edge.destination, edge.kind, edge.position};
  if (excludedKeys_.insert(key).second) {
    result_.excluded.push_back(
        {edge.source, edge.destination, std::move(reason)});
    const auto known =
        std::ranges::any_of(result_.excludedNodes, [&](const auto &item) {
          return item.id == edge.destination;
        });
    if (!known)
      result_.excludedNodes.push_back(
          {edge.destination, result_.excluded.back().reason});
  }
}

} // namespace facts::callgraph
