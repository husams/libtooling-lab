#include "commands/analyse/CallGraphRecoveryState.h"
#include "commands/analyse/RecoveryScan.h"
#include <algorithm>
#include <queue>

namespace facts::commands {
std::vector<SymbolId> recoveryReachable(const callgraph::QueryGraph &graph,
                                        std::span<const SymbolId> roots,
                                        std::optional<unsigned> maxDepth) {
  std::set<SymbolId> seen(roots.begin(), roots.end());
  std::queue<std::pair<SymbolId, unsigned>> pending;
  for (const auto id : roots)
    pending.emplace(id, 0);
  while (!pending.empty()) {
    const auto [source, depth] = pending.front();
    pending.pop();
    if (maxDepth && depth >= *maxDepth)
      continue;
    for (const auto &edge : graph.edges)
      if (edge.source == source && seen.insert(edge.destination).second)
        pending.emplace(edge.destination, depth + 1);
  }
  return {seen.begin(), seen.end()};
}

void preserveRecoveryEvidence(RecoveryContext &context,
                              const RecoveryResult &result,
                              std::span<const SymbolId> roots,
                              bool freshlyPublished,
                              std::optional<unsigned> maxDepth) {
  const auto reachable = recoveryReachable(result.graph, roots, maxDepth);
  for (const auto &node : result.graph.nodes) {
    if (!std::ranges::contains(reachable, node.id))
      continue;
    const auto state =
        result.coverage ? callgraph::extractionCoverage(*result.coverage, node)
                        : "unknown";
    if ((freshlyPublished && node.bodyEvidence) ||
        (state != "stale" && (node.implicit || state == "complete")))
      context.preservedUsrs.insert(node.usr);
  }
}

bool retainRecoveryInputs(RecoveryContext &before, RecoveryContext &after) {
  bool unchanged =
      recoveryRegistryFingerprint(before) == recoveryRegistryFingerprint(after);
  after.digests = std::move(before.digests);
  for (const auto &[id, scan] : before.scans) {
    if (!recoveryScanCurrent(after, *scan)) {
      unchanged = false;
      continue;
    }
    after.scans[id] = scan;
    if (scan->completeInputs) {
      after.inputClosures[id] = scan->inputs;
      after.closureDigests[id] = scan->digest;
    }
  }
  if (unchanged) {
    after.preservedUsrs = std::move(before.preservedUsrs);
    after.reusedUsrs = std::move(before.reusedUsrs);
    after.reusedOwners = std::move(before.reusedOwners);
  }
  return unchanged;
}

void recoveryFailure(RecoveryReport &report, std::string reason) {
  RecoveryEntry failure;
  failure.reason = std::move(reason);
  report.attempted.push_back(failure);
  report.failed.push_back(std::move(failure));
}
} // namespace facts::commands
