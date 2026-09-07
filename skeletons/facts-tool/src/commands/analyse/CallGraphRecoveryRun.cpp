#include "analysis/callgraph/CallGraphSelection.h"
#include "commands/analyse/CallGraphCancellation.h"
#include "commands/analyse/CallGraphRecovery.h"
#include "commands/analyse/CallGraphResult.h"
#include "commands/analyse/CallGraphRun.h"
#include <iostream>

namespace facts::commands {
std::expected<int, std::string>
runSelectedCallGraph(const cli::CallGraphOptions &options,
                     const CallGraphRequest &request,
                     const callgraph::QueryGraph &graph,
                     const callgraph::CoverageReport *coverage) {
  if (!options.recoverMissing && options.all && graph.edges.empty())
    return std::unexpected("facts database contains no call facts");
  if (!options.recoverMissing)
    return runCallGraphQuery(options, request, graph, coverage, nullptr);
  auto roots = callgraph::selectRoots(graph, options.function, options.all);
  if (!roots)
    return std::unexpected("facts-tool: usage error: " + roots.error());
  // Validate selectors and controls before any recovery write or artifact.
  if (options.target) {
    auto target = callgraph::selectOne(graph, *options.target, "target");
    if (!target)
      return std::unexpected("facts-tool: usage error: " + target.error());
  }
  auto controls = makeCallGraphRequest(options, coverage, {});
  if (!controls)
    return std::unexpected(controls.error());
  std::optional<CallGraphResult> initial;
  auto observe = [&](const RecoveryResult &current)
      -> std::expected<std::vector<SymbolId>, std::string> {
    auto *evidence = current.coverage ? &*current.coverage : nullptr;
    auto queried = queryCallGraph(options, request, current.graph, evidence);
    if (!queried)
      return std::unexpected(queried.error());
    initial = std::move(*queried);
    if (options.format == "mermaid" && !options.output.empty()) {
      auto published =
          publishCallGraph(options, request, current.graph, evidence, *initial,
                           &current.report, true);
      if (!published)
        return std::unexpected(published.error());
      std::cerr << "facts-tool: initial graph published\n";
    }
    return initial->traversal.nodes;
  };
  std::vector<SymbolId> rootIds;
  for (const auto *root : *roots)
    rootIds.push_back(root->id);
  auto recovered = recoverCallGraph(
      options, graph, coverage ? std::optional{*coverage} : std::nullopt,
      rootIds, observe);
  if (!recovered)
    return std::unexpected(recovered.error());
  auto *evidence = recovered->coverage ? &*recovered->coverage : nullptr;
  if (CallGraphCancellation::cancelled()) {
    initial->traversal.reason = "cancelled";
    initial->traversal.truncated = std::max(1U, initial->traversal.truncated);
    for (const auto *root : initial->roots)
      initial->traversal.frontier.push_back({root->id, "cancelled"});
  }
  auto published = publishCallGraph(options, request, recovered->graph,
                                    evidence, *initial, &recovered->report);
  if (!published)
    return published;
  if (*published == 130)
    return published;
  if (!recovered->report.failed.empty())
    return std::unexpected("recovery-failed");
  return published;
}
} // namespace facts::commands
