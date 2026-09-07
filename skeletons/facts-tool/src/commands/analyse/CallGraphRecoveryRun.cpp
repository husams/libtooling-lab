#include "analysis/callgraph/CallGraphSelection.h"
#include "commands/analyse/CallGraphCancellation.h"
#include "commands/analyse/CallGraphOutcome.h"
#include "commands/analyse/CallGraphRecovery.h"
#include "commands/analyse/CallGraphResult.h"
#include "commands/analyse/CallGraphRun.h"
#include <memory>

namespace facts::commands {
namespace {
// Results point into their graph, so each generation retains its own copy.
struct CallGraphSnapshot {
  callgraph::QueryGraph graph;
  std::optional<callgraph::CoverageReport> coverage;
  CallGraphResult result;
};

std::expected<std::unique_ptr<CallGraphSnapshot>, std::string>
snapshotCallGraph(const cli::CallGraphOptions &options,
                  const CallGraphRequest &request,
                  const RecoveryResult &current) {
  auto snapshot = std::make_unique<CallGraphSnapshot>(
      current.graph, current.coverage, CallGraphResult{});
  const auto *coverage = snapshot->coverage ? &*snapshot->coverage : nullptr;
  return queryCallGraph(options, request, snapshot->graph, coverage)
      .transform([&](auto result) {
        snapshot->result = std::move(result);
        return std::move(snapshot);
      });
}
} // namespace

std::expected<CallGraphRunRecord, std::string>
runRecoveredCallGraph(const cli::CallGraphOptions &options,
                      const CallGraphRequest &request,
                      const callgraph::QueryGraph &graph,
                      const callgraph::CoverageReport *coverage) {
  auto roots = callgraph::selectRoots(graph, options.function, options.all);
  if (!roots)
    return std::unexpected("facts-tool: usage error: " + roots.error());
  std::unique_ptr<CallGraphSnapshot> snapshot;
  auto observe = [&](const RecoveryResult &current)
      -> std::expected<std::vector<SymbolId>, std::string> {
    // Once interrupted, keep the last usable generation instead of
    // publishing an empty traversal as the final result.
    if (snapshot && CallGraphCancellation::cancelled())
      return snapshot->result.traversal.nodes;
    auto queried = snapshotCallGraph(options, request, current);
    if (!queried)
      return std::unexpected(queried.error());
    if (snapshot && (*queried)->result.traversal.reason == "cancelled")
      return snapshot->result.traversal.nodes;
    snapshot = std::move(*queried);
    return snapshot->result.traversal.nodes;
  };
  std::vector<SymbolId> rootIds;
  for (const auto *root : *roots)
    rootIds.push_back(root->id);
  auto recovered = recoverCallGraph(
      options, graph, coverage ? std::optional{*coverage} : std::nullopt,
      rootIds, observe);
  std::string error;
  if (!recovered)
    error = recovered.error();
  if (!snapshot) {
    // The first traversal itself failed: record what was reached (nothing).
    RecoveryResult initial{graph, std::nullopt, {}};
    auto queried = snapshotCallGraph(options, request, initial);
    if (!queried)
      return std::unexpected(queried.error());
    snapshot = std::move(*queried);
  }
  auto &final = snapshot->result;
  const bool cancelled = CallGraphCancellation::cancelled();
  if (cancelled && final.traversal.reason != "cancelled") {
    final.traversal.reason = "cancelled";
    final.traversal.truncated = std::max(1U, final.traversal.truncated);
    for (const auto *root : final.roots)
      final.traversal.frontier.push_back({root->id, "cancelled"});
  }
  return makeCallGraphRunRecord(options, request, final,
                                recovered ? &recovered->report : nullptr,
                                cancelled, std::move(error));
}
} // namespace facts::commands
