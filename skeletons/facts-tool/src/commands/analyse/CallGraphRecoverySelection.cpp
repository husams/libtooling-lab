#include "commands/analyse/CallGraphRecoverySelection.h"
#include <algorithm>

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
    const auto fileId =
        node.definitionLocation ? node.definitionLocation->file : node.id.file;
    const auto file = context.files.find(fileId);
    if (file == context.files.end() || !file->second.component.repositoryId)
      continue;
    if (context.preservedUsrs.contains(node.usr))
      continue;
    const auto state =
        coverage ? callgraph::extractionCoverage(*coverage, node) : "unknown";
    if (state == "complete")
      continue;
    if (node.implicit && state != "stale")
      continue;
    if (node.usr.empty())
      continue;
    auto ids = indexedFiles(context, node.usr);
    if (node.definitionLocation &&
        context.commands.contains(node.definitionLocation->file))
      ids.push_back(node.definitionLocation->file);
    preferred.insert(ids.begin(), ids.end());
    const auto fallback = fallbackFiles(context);
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
    std::error_code aError, bError;
    const bool aExists = std::filesystem::is_regular_file(a.source, aError);
    const bool bExists = std::filesystem::is_regular_file(b.source, bError);
    if (aExists != bExists)
      return aExists;
    return preferred.contains(a.entry.tuFileId) >
           preferred.contains(b.entry.tuFileId);
  });
  return result;
}
} // namespace facts::commands
