#include "analysis/callgraph/CallGraphSearchBudget.h"
#include "analysis/callgraph/CallGraphScope.h"
#include <algorithm>
#include <ranges>

namespace facts::callgraph::detail {
SearchBudget::SearchBudget(TraversalRequest request,
                           const CoverageReport *coverage,
                           TraversalResult &result)
    : request_(std::move(request)), coverage_(coverage), result_(result),
      started_(std::chrono::steady_clock::now()) {
  result_.scope = request_.scope;
  result_.limits = request_.limits;
}

void SearchBudget::truncate(SymbolId id, std::string reason) {
  if (!std::ranges::any_of(result_.frontier, [&](const auto &item) {
        return item.id == id && item.reason == reason;
      }))
    result_.frontier.push_back({id, reason});
  if (result_.reason.empty() || reason != "max_depth")
    result_.reason = reason;
  result_.truncated = static_cast<unsigned>(result_.frontier.size());
}

bool SearchBudget::stopped(SymbolId frontier) {
  if (!result_.reason.empty() && result_.reason != "max_depth") {
    truncate(frontier, result_.reason);
    return true;
  }
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

bool SearchBudget::include(const QueryNode &node, const QueryEdge *edge) {
  const auto reason = exclusionReason(node, request_.scope, coverage_);
  if (reason.empty())
    return true;
  if (!std::ranges::any_of(result_.excludedNodes, [&](const auto &item) {
        return item.id == node.id;
      }))
    result_.excludedNodes.push_back({node.id, reason});
  if (edge &&
      excluded_
          .insert({edge->source, edge->destination, edge->kind, edge->position})
          .second)
    result_.excluded.push_back(
        {edge->source, edge->destination, reason, node.id});
  return false;
}

bool SearchBudget::admit(const QueryNode &node, const QueryEdge *edge) {
  const bool newNode = !std::ranges::contains(result_.nodes, node.id);
  if (newNode && request_.limits.nodes &&
      result_.nodes.size() >= *request_.limits.nodes) {
    truncate(node.id, "max_nodes");
    return false;
  }
  const Key key =
      edge ? Key{edge->source, edge->destination, edge->kind, edge->position}
           : Key{};
  if (edge && !edges_.contains(key) && request_.limits.edges &&
      edges_.size() >= *request_.limits.edges) {
    truncate(node.id, "max_edges");
    return false;
  }
  if (newNode)
    result_.nodes.push_back(node.id);
  if (edge)
    edges_.insert(key);
  return true;
}

bool SearchBudget::root(const QueryNode &node) {
  return include(node) && !stopped(node.id) && admit(node);
}
} // namespace facts::callgraph::detail
