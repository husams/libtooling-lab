#include "analysis/callgraph/CallGraphSelection.h"
#include "commands/analyse/CallGraphRecovery.h"
#include "commands/analyse/CallGraphRun.h"

namespace facts::commands {
std::expected<int, std::string>
runSelectedCallGraph(const cli::CallGraphOptions &options,
                     const CallGraphRequest &request,
                     const callgraph::QueryGraph &graph,
                     const callgraph::CoverageReport *coverage) {
  if (!options.recoverMissing)
    return runCallGraphQuery(options, request, graph, coverage, nullptr);
  auto roots = callgraph::selectRoots(graph, options.function, options.all);
  if (!roots)
    return std::unexpected("facts-tool: usage error: " + roots.error());
  std::vector<SymbolId> rootIds;
  for (const auto *root : *roots)
    rootIds.push_back(root->id);
  auto recovered = recoverCallGraph(
      options, graph, coverage ? std::optional{*coverage} : std::nullopt,
      rootIds);
  if (!recovered)
    return std::unexpected(recovered.error());
  auto rendered =
      runCallGraphQuery(options, request, recovered->graph,
                        recovered->coverage ? &*recovered->coverage : nullptr,
                        &recovered->report);
  if (!rendered)
    return rendered;
  if (!recovered->report.failed.empty())
    return std::unexpected("recovery-failed");
  return rendered;
}
} // namespace facts::commands
