#pragma once

#include "analysis/callgraph/CallGraphContext.h"
#include "analysis/callgraph/CallGraphTraversal.h"

#include <chrono>
#include <set>
#include <tuple>

namespace facts::callgraph {

class TraversalEngine {
public:
  TraversalEngine(const QueryGraph &graph, TraversalRequest request,
                  const CoverageReport *coverage);
  RenderedGraph run(const std::vector<const QueryNode *> &roots);

private:
  using EdgeKey = std::tuple<SymbolId, SymbolId, RelationKind, int>;
  const QueryNode *findNode(SymbolId id) const;
  bool allowed(const QueryNode &node) const;
  bool stopRequested(SymbolId frontier);
  bool admitNode(SymbolId id);
  bool admit(const QueryEdge &edge, SymbolId target);
  bool hasOutgoing(const QueryNode &node, const QueryContext &context) const;
  void exclude(const QueryEdge &edge, std::string reason);
  void truncate(SymbolId id, std::string reason);
  void walk(const QueryNode &source, const QueryContext &context, int depth,
            std::set<QueryContext> path);

  const QueryGraph &graph_;
  TraversalRequest request_;
  const CoverageReport *coverage_;
  std::chrono::steady_clock::time_point started_;
  std::set<QueryContext> expanded_;
  std::set<EdgeKey> edgeKeys_;
  std::set<EdgeKey> excludedKeys_;
  RenderedGraph result_;
};

} // namespace facts::callgraph
