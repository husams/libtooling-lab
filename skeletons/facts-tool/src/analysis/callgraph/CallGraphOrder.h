#pragma once

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphTraversal.h"

#include <algorithm>
#include <ranges>
#include <tuple>

namespace facts::callgraph::detail {

inline const QueryNode *findSearchNode(const QueryGraph &graph, SymbolId id) {
  const auto found = std::ranges::find(graph.nodes, id, &QueryNode::id);
  return found == graph.nodes.end() ? nullptr : &*found;
}

inline auto edgeKey(const QueryEdge &edge) {
  return std::tuple{
      edge.source,   edge.destination, static_cast<unsigned>(edge.kind),
      edge.position, edge.file,        edge.offset};
}

inline bool edgeLess(const QueryGraph &graph, const QueryEdge *left,
                     const QueryEdge *right, bool reverse = false) {
  const auto *leftNode =
      findSearchNode(graph, reverse ? left->source : left->destination);
  const auto *rightNode =
      findSearchNode(graph, reverse ? right->source : right->destination);
  return std::tuple{leftNode ? leftNode->usr : std::string{}, edgeKey(*left)} <
         std::tuple{rightNode ? rightNode->usr : std::string{},
                    edgeKey(*right)};
}

inline std::vector<const QueryEdge *>
orderedEdges(const QueryGraph &graph, SymbolId id, bool reverse = false) {
  std::vector<const QueryEdge *> result;
  for (const auto &edge : graph.edges)
    if ((reverse ? edge.destination : edge.source) == id)
      result.push_back(&edge);
  std::ranges::sort(result, [&](const auto *left, const auto *right) {
    return edgeLess(graph, left, right, reverse);
  });
  return result;
}

inline std::pair<bool, bool> boundaries(const QueryNode &node,
                                        const CoverageReport *coverage) {
  if (!coverage)
    return {node.external || !hasDefinitionEvidence(node), false};
  return {!hasDefinitionEvidence(node) && !isProjectLocal(*coverage, node),
          !hasDefinitionEvidence(node) && isProjectLocal(*coverage, node)};
}

inline void recordNode(std::vector<SymbolId> &nodes, SymbolId id) {
  if (std::ranges::find(nodes, id) == nodes.end())
    nodes.push_back(id);
}

inline bool sameEdge(const TraversedEdge &value, const QueryEdge &edge) {
  return edgeKey(value.edge) == edgeKey(edge);
}

inline void recordEdge(std::vector<TraversedEdge> &edges, const QueryEdge &edge,
                       int depth, bool cycle, bool reused, bool capped,
                       std::pair<bool, bool> boundary) {
  if (!std::ranges::any_of(
          edges, [&](const auto &value) { return sameEdge(value, edge); }))
    edges.push_back(
        {edge, depth, cycle, reused, boundary.first, boundary.second, capped});
}

} // namespace facts::callgraph::detail
