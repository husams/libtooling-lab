#pragma once
#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphSearch.h"
#include "commands/analyse/CallGraphRequest.h"

namespace facts::commands {
// One traversal generation: root/target pointers stay valid only as long as
// the graph they were selected from.
struct CallGraphResult {
  std::vector<const callgraph::QueryNode *> roots;
  const callgraph::QueryNode *target = nullptr;
  callgraph::TraversalResult traversal;
  std::vector<callgraph::QueryPath> paths;
};

std::expected<CallGraphResult, std::string>
queryCallGraph(const cli::CallGraphOptions &options,
               const CallGraphRequest &request,
               const callgraph::QueryGraph &graph,
               const callgraph::CoverageReport *coverage);
} // namespace facts::commands
