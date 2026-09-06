#pragma once

#include "analysis/callgraph/CallGraphContext.h"
#include "analysis/callgraph/CallGraphSearch.h"

namespace facts::callgraph::detail {

struct PathState {
  QueryPath path;
  QueryContext context;
};

class PathSearch {
public:
  PathSearch(const QueryGraph &graph, const QueryNode &target,
             std::optional<int> maxDepth, const CoverageReport *coverage);
  QuerySearch run(const QueryNode &source, PathMode mode);

private:
  bool capped(const PathState &state) const;
  void visit(const PathState &state, const QueryEdge &edge,
             const QueryNode &node, bool reused = false);
  std::vector<const QueryEdge *> eligible(const PathState &state) const;
  PathState descend(const PathState &state, const QueryEdge &edge) const;
  void shortest(PathState initial);
  void allSimple(PathState state);
  void retainPaths();

  const QueryGraph &graph_;
  const QueryNode &target_;
  std::optional<int> maxDepth_;
  const CoverageReport *coverage_;
  QuerySearch result_;
};

} // namespace facts::callgraph::detail
