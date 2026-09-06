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
  preserveRecoveryEvidence(*context, result, roots);
  for (const auto &node : result.graph.nodes)
    if (context->preservedUsrs.contains(node.usr)) {
      const auto file = node.definitionLocation ? node.definitionLocation->file
                                                : node.id.file;
      result.report.reused.push_back(makeEntry(
          *context, file, {node.usr}, "existing valid body and calls"));
    }
  recovery::AttemptCache cache;
  recovery::InputDigestCache digests;
  auto inputVersion = recoveryInputVersion(*context, digests);
  while (true) {
    const auto reachable = recoveryReachable(result.graph, roots);
    auto candidates = selectRecoveryCandidates(
        *context, result.graph, result.coverage ? &*result.coverage : nullptr,
        reachable);
    if (!candidates) {
      recoveryFailure(result.report, candidates.error());
      break;
    }
    auto extracted =
        processRecoveryCandidates(*context, options, std::move(*candidates),
                                  result.report, cache, context->preservedUsrs);
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
    auto preserved = std::move(context->preservedUsrs);
    *context = std::move(*currentContext);
    auto currentVersion = recoveryInputVersion(*context, digests);
    if (inputVersion && currentVersion && *inputVersion == *currentVersion)
      context->preservedUsrs = std::move(preserved);
    else {
      for (auto &node : result.graph.nodes)
        node.bodyEvidence = false;
      result.coverage.reset();
    }
    inputVersion = std::move(currentVersion);
    if (result.coverage) {
      auto updated = callgraph::loadCoverage(context->project, result.graph);
      if (!updated) {
        recoveryFailure(result.report, updated.error());
        break;
      }
      result.coverage = std::move(*updated);
    }
    preserveRecoveryEvidence(*context, result, roots, true);
  }
  std::cerr << "facts-tool: recovery-complete\n";
  return result;
}
} // namespace facts::commands
