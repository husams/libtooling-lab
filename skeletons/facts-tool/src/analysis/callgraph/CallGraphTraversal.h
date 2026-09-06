#pragma once

#include "analysis/callgraph/CallGraphQuery.h"
#include "analysis/callgraph/CallGraphSemantics.h"

#include <optional>
#include <string>
#include <vector>

namespace facts::callgraph {

struct CoverageReport;

struct TraversedEdge {
  QueryEdge edge;
  int depth = 0;
  bool cycle = false;
  bool reused = false;
  bool externalBoundary = false;
  bool definitionBoundary = false;
  bool depthTruncated = false;
};

struct RenderedGraph {
  std::string text;
  unsigned truncated = 0;
  std::vector<SymbolId> nodes;
  std::vector<TraversedEdge> edges;
};

RenderedGraph renderCallGraph(const QueryGraph &graph,
                              const std::vector<const QueryNode *> &roots,
                              std::optional<int> maxDepth,
                              const CoverageReport *coverage = nullptr,
                              EdgeView view = EdgeView::Semantic);

} // namespace facts::callgraph
