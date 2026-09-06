#include "commands/analyse/CallGraphRecoverySelection.h"
#include "tooling/CompilationCommandCodec.h"
#include <algorithm>
#include <queue>
#include <ranges>

namespace facts::commands {
std::expected<std::vector<RecoveryCandidate>, std::string>
selectRecoveryCandidates(const RecoveryContext &context,
                         const callgraph::QueryGraph &graph,
                         const callgraph::CoverageReport *coverage,
                         std::span<const SymbolId> reachable) {
  std::map<FileId, std::set<std::string>> wanted;
  std::set<FileId> preferred;
  for (const auto &node : graph.nodes) {
    if (!std::ranges::contains(reachable, node.id))
      continue;
    if (context.preservedUsrs.contains(node.usr))
      continue;
    const auto state =
        coverage ? callgraph::extractionCoverage(*coverage, node) : "unknown";
    if (state == "complete")
      continue;
    if ((node.implicit || node.bodyEvidence) && state != "stale")
      continue;
    if (node.usr.empty())
      continue;
    auto ids = indexedFiles(context, node.usr);
    preferred.insert(ids.begin(), ids.end());
    const auto fallback = fallbackFiles(context, node.id.file);
    ids.insert(ids.end(), fallback.begin(), fallback.end());
    for (const auto id : ids)
      wanted[id].insert(node.usr);
  }
  std::vector<RecoveryCandidate> result;
  for (const auto &[id, usrs] : wanted) {
    const auto command = context.commands.find(id);
    if (command == context.commands.end())
      continue;
    auto path = command->second.path;
    result.push_back(
        {makeEntry(context, id,
                   std::vector<std::string>(usrs.begin(), usrs.end()),
                   "missing body or call evidence"),
         std::move(path)});
  }
  std::ranges::stable_sort(result, [&](const auto &a, const auto &b) {
    return preferred.contains(a.entry.tuFileId) >
           preferred.contains(b.entry.tuFileId);
  });
  return result;
}
} // namespace facts::commands
