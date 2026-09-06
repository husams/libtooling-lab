#include "commands/analyse/CallGraphRecoveryState.h"
#include <algorithm>
#include <queue>

namespace facts::commands {
std::vector<SymbolId> recoveryReachable(const callgraph::QueryGraph &graph,
                                        std::span<const SymbolId> roots) {
  std::set<SymbolId> seen(roots.begin(), roots.end());
  std::queue<SymbolId> pending;
  for (const auto id : roots)
    pending.push(id);
  while (!pending.empty()) {
    const auto source = pending.front();
    pending.pop();
    for (const auto &edge : graph.edges)
      if (edge.source == source && seen.insert(edge.destination).second)
        pending.push(edge.destination);
  }
  return {seen.begin(), seen.end()};
}

void preserveRecoveryEvidence(RecoveryContext &context,
                              const RecoveryResult &result,
                              std::span<const SymbolId> roots,
                              bool freshlyPublished) {
  const auto reachable = recoveryReachable(result.graph, roots);
  for (const auto &node : result.graph.nodes) {
    if (!std::ranges::contains(reachable, node.id))
      continue;
    const auto state =
        result.coverage ? callgraph::extractionCoverage(*result.coverage, node)
                        : "unknown";
    if ((freshlyPublished && node.bodyEvidence) ||
        (state != "stale" &&
         (node.bodyEvidence || node.implicit || state == "complete")))
      context.preservedUsrs.insert(node.usr);
  }
}

std::expected<std::string, std::string>
recoveryInputVersion(const RecoveryContext &context,
                     recovery::InputDigestCache &digests) {
  if (context.commands.empty())
    return recoveryRegistryFingerprint(context);
  const auto &[id, command] = *context.commands.begin();
  RecoveryCandidate candidate;
  candidate.entry.tuFileId = id;
  candidate.source = command.path;
  return digests.digest(recoveryRegisteredInputs(context, candidate))
      .transform([&](const auto &digest) {
        return recoveryRegistryFingerprint(context) + ":" + digest;
      })
      .transform_error([](const auto &error) { return error.message; });
}

void recoveryFailure(RecoveryReport &report, std::string reason) {
  RecoveryEntry failure;
  failure.reason = std::move(reason);
  report.attempted.push_back(failure);
  report.failed.push_back(std::move(failure));
}
} // namespace facts::commands
