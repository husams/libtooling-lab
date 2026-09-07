#include "commands/analyse/CallGraphCommand.h"
#include "commands/analyse/CallGraphCancellation.h"
#include "commands/analyse/CallGraphCompletion.h"
#include "commands/analyse/CallGraphRequest.h"
#include "commands/analyse/CallGraphRun.h"
#include "commands/analyse/CallGraphRunStore.h"
#include "commands/analyse/CallGraphSession.h"

namespace facts::commands {
namespace {
// Everything before the first traversal step: usage (2), configuration (3)
// and database (1) failures plus a pre-traversal interrupt (130) return
// here without writing a run or printing a completion line.
std::expected<CallGraphRunRecord, std::string>
traverse(const cli::CallGraphOptions &options, const CallGraphRequest &request,
         const callgraph::QueryGraph &graph,
         const callgraph::CoverageReport *coverage) {
  if (!options.recoverMissing && options.all && graph.edges.empty())
    return std::unexpected("facts database contains no call facts");
  if (auto selected =
          validateCallGraphSelection(options, request, graph, coverage);
      !selected)
    return std::unexpected(selected.error());
  if (CallGraphCancellation::cancelled())
    return std::unexpected("facts-tool: cancelled before traversal");
  return options.recoverMissing
             ? runRecoveredCallGraph(options, request, graph, coverage)
             : runStoredCallGraph(options, request, graph, coverage);
}

std::expected<CallGraphRunRecord, std::string>
runResolvedGraph(const cli::CallGraphOptions &options,
                 const CallGraphRequest &request) {
  if (options.recoverMissing && options.configuration.empty())
    return std::unexpected("facts-tool: configuration error: recovery "
                           "requires a project configuration");
  return callgraph::loadCallGraph(options.facts)
      .and_then([&](const auto &graph)
                    -> std::expected<CallGraphRunRecord, std::string> {
        std::optional<callgraph::CoverageReport> coverage;
        if (!options.configuration.empty()) {
          auto loaded = callgraph::loadCoverage(options.configuration, graph);
          if (!loaded)
            return std::unexpected(loaded.error());
          coverage = std::move(*loaded);
        }
        return traverse(options, request, graph,
                        coverage ? &*coverage : nullptr);
      });
}
} // namespace

std::expected<int, std::string>
runCallGraph(const cli::CallGraphOptions &options) {
  CallGraphCancellation cancellation;
  return validateCallGraphRequest(options)
      .and_then([&](const auto &request) {
        return resolveGraphSession(options).and_then([&](const auto &resolved) {
          return runResolvedGraph(resolved, request);
        });
      })
      .and_then([](const CallGraphRunRecord &record) {
        // The completion line is printed only after the run has committed.
        return persistCallGraphRun(record).and_then([&](std::int64_t runId) {
          return completeCallGraphRun(record, runId);
        });
      });
}
} // namespace facts::commands
