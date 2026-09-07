#pragma once
#include "analysis/callgraph/CallGraphRecoveryTypes.h"
#include "commands/analyse/CallGraphRequest.h"

namespace facts::commands {
struct CallGraphResult {
  std::vector<const callgraph::QueryNode *> roots;
  const callgraph::QueryNode *target = nullptr;
  callgraph::RenderedGraph traversal;
  std::vector<callgraph::QueryPath> paths;
  std::string pathResult;
};

std::expected<CallGraphResult, std::string>
queryCallGraph(const cli::CallGraphOptions &options,
               const CallGraphRequest &request,
               const callgraph::QueryGraph &graph,
               const callgraph::CoverageReport *coverage);

std::expected<int, std::string> publishCallGraph(
    const cli::CallGraphOptions &options, const CallGraphRequest &request,
    const callgraph::QueryGraph &graph,
    const callgraph::CoverageReport *coverage, const CallGraphResult &result,
    const callgraph::RecoveryReport *recovery = nullptr, bool initial = false);
} // namespace facts::commands
