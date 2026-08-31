#pragma once

#include "analysis/callgraph/CallGraphQuery.h"

#include <optional>
#include <string>

namespace facts::callgraph {

struct RenderedGraph {
  std::string text;
  unsigned truncated = 0;
};

RenderedGraph renderCallGraph(const QueryGraph &graph,
                              const std::vector<const QueryNode *> &roots,
                              std::optional<int> maxDepth);

} // namespace facts::callgraph
