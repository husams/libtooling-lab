#include "commands/analyse/CallGraphRecoverySelection.h"
#include "commands/analyse/CallGraphRecoveryState.h"
#include <iostream>

namespace facts::commands {
std::expected<RecoveryResult, std::string>
recoverCallGraph(const cli::CallGraphOptions &options,
                 callgraph::QueryGraph graph,
                 std::optional<callgraph::CoverageReport> coverage,
                 std::span<const SymbolId> roots) {
  RecoveryResult result{std::move(graph), std::move(coverage), {}};
  result.report.requested = true;
  std::cerr << "facts-tool: recovery-start\n";
  auto context = loadRecoveryContext(options);
  if (!context) {
    recoveryFailure(result.report, context.error());
    std::cerr << "facts-tool: recovery-complete\n";
    return result;
  }
  preserveRecoveryEvidence(*context, result, roots, false, options.maxDepth);
  context->reusedUsrs = context->preservedUsrs;
  recovery::AttemptCache cache;
  while (true) {
    const auto reachable =
        recoveryReachable(result.graph, roots, options.maxDepth);
    auto candidates = selectRecoveryCandidates(
        *context, result.graph, result.coverage ? &*result.coverage : nullptr,
        reachable);
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
  }
  result.report.reused = collectRecoveryReuseReport(*context, result.graph);
  std::cerr << "facts-tool: recovery-complete\n";
  return result;
}
} // namespace facts::commands
