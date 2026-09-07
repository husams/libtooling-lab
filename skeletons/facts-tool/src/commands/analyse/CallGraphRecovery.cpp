#include "commands/analyse/CallGraphCancellation.h"
#include "commands/analyse/CallGraphRecoverySelection.h"
#include "commands/analyse/CallGraphRecoveryState.h"
#include "cli/Verbose.h"

namespace facts::commands {
std::expected<RecoveryResult, std::string>
recoverCallGraph(const cli::CallGraphOptions &options,
                 callgraph::QueryGraph graph,
                 std::optional<callgraph::CoverageReport> coverage,
                 std::span<const SymbolId> roots, RecoveryObserver observe) {
  RecoveryResult result{std::move(graph), std::move(coverage), {}};
  auto reachable =
      observe ? observe(result)
              : std::expected<std::vector<SymbolId>, std::string>{
                    recoveryReachable(result.graph, roots, options.maxDepth)};
  if (!reachable)
    return std::unexpected(reachable.error());
  if (CallGraphCancellation::cancelled())
    return result;
  cli::logVerbose(options.verbosity, 1, "facts-tool: recovery-start");
  auto context = loadRecoveryContext(options);
  if (!context) {
    recoveryFailure(result.report, context.error());
    cli::logVerbose(options.verbosity, 1, "facts-tool: recovery-complete");
    return result;
  }
  preserveRecoveryEvidence(*context, result, roots, false, options.maxDepth);
  context->reusedUsrs = context->preservedUsrs;
  recovery::AttemptCache cache;
  while (!CallGraphCancellation::cancelled()) {
    auto candidates = selectRecoveryCandidates(
        *context, result.graph, result.coverage ? &*result.coverage : nullptr,
        *reachable);
    if (!candidates) {
      recoveryFailure(result.report, candidates.error());
      break;
    }
    auto extracted =
        processRecoveryCandidates(*context, options, std::move(*candidates),
                                  result.report, cache, result.graph);
    if (!extracted) {
      recoveryFailure(result.report, extracted.error());
      break;
    }
    if (!*extracted)
      break;
    auto refreshed = callgraph::loadCallGraph(options.facts);
    auto currentContext = loadRecoveryContext(options);
    if (!refreshed || !currentContext) {
      recoveryFailure(result.report,
                      !refreshed ? refreshed.error() : currentContext.error());
      break;
    }
    result.graph = std::move(*refreshed);
    if (!retainRecoveryInputs(*context, *currentContext)) {
      for (auto &node : result.graph.nodes)
        node.bodyEvidence = false;
      result.coverage.reset();
    }
    *context = std::move(*currentContext);
    if (result.coverage) {
      auto updated = callgraph::loadCoverage(context->project, result.graph);
      if (!updated) {
        recoveryFailure(result.report, updated.error());
        break;
      }
      result.coverage = std::move(*updated);
    }
    preserveRecoveryEvidence(*context, result, roots, true, options.maxDepth);
    // Assign each branch separately: GCC misreads the ternary's temporary
    // std::expected as a freed non-heap object.
    if (observe)
      reachable = observe(result);
    else
      reachable = recoveryReachable(result.graph, roots, options.maxDepth);
    if (!reachable)
      return std::unexpected(reachable.error());
  }
  result.report.reused = collectRecoveryReuseReport(*context, result.graph);
  cli::logVerbose(options.verbosity, 1, "facts-tool: recovery-complete");
  return result;
}
} // namespace facts::commands
