#pragma once
#include "analysis/callgraph/CallGraphTraversal.h"
#include <set>
#include <tuple>

namespace facts::callgraph::detail {
class SearchBudget {
public:
  SearchBudget(TraversalRequest request, const CoverageReport *coverage,
               RenderedGraph &result);
  bool root(const QueryNode &node);
  bool include(const QueryNode &node, const QueryEdge *edge = nullptr);
  bool admit(const QueryNode &node, const QueryEdge *edge = nullptr);
  bool stopped(SymbolId frontier);
  void truncate(SymbolId frontier, std::string reason);

private:
  using Key = std::tuple<SymbolId, SymbolId, RelationKind, unsigned>;
  TraversalRequest request_;
  const CoverageReport *coverage_;
  RenderedGraph &result_;
  std::chrono::steady_clock::time_point started_;
  std::set<Key> edges_;
  std::set<Key> excluded_;
};
} // namespace facts::callgraph::detail
