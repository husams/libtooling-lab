#pragma once

#include "analysis/callgraph/CallGraphContext.h"
#include "analysis/callgraph/CallGraphSearch.h"
#include "analysis/callgraph/CallGraphSearchBudget.h"

namespace facts::callgraph::detail {

struct PathState {
  QueryPath path;
  QueryContext context;
};

class PathSearch {
public:
  PathSearch(const QueryGraph &graph, const QueryNode &target,
             TraversalRequest request, const CoverageReport *coverage);
  QuerySearch run(const QueryNode &source, PathMode mode);

private:
  bool capped(const PathState &state) const;
  bool visit(const PathState &state, const QueryEdge &edge,
             const QueryNode &node, bool reused = false);
  std::vector<const QueryEdge *> eligible(const PathState &state);
  PathState descend(const PathState &state, const QueryEdge &edge) const;
  void shortest(PathState initial);
  void allSimple(PathState state);
  void retainPaths();

  const QueryGraph &graph_;
  const QueryNode &target_;
  std::optional<int> maxDepth_;
  const CoverageReport *coverage_;
  QuerySearch result_;
  SearchBudget budget_;
};

} // namespace facts::callgraph::detail
