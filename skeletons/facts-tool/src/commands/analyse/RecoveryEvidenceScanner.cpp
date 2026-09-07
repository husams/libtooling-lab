#include "commands/analyse/RecoveryEvidenceScanner.h"
#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "commands/analyse/RecoveryScan.h"
#include "storage/catalog/File.h"

namespace facts::commands {
void collectRecoveryScanBodies(const RecoveryContext &context,
                               RecoveryScan &scan) {
  auto graph = collectRecoveryNativeFacts(context, scan);
  if (!graph) {
    scan.facts.unsupported = true;
    scan.error = graph.error();
    return;
  }
  std::map<SymbolId, std::string> usrs;
  for (const auto &node : graph->nodes) {
    usrs[node.id] = node.usr;
    if (!node.definitionLocation)
      continue;
    const auto &location = *node.definitionLocation;
    const auto file = context.files.find(location.file);
    if (file == context.files.end())
      continue;
    const auto path = catalog::filePath(file->second);
    if (!path)
      continue;
    scan.facts.definitions[node.usr] = {path->string(), location.offset,
                                        location.size};
    scan.facts.calls.try_emplace(node.usr);
    scan.facts.unresolved[node.usr] = node.unresolved;
  }
  for (const auto &edge : graph->edges) {
    if (edge.kind != RelationKind::Calls)
      continue;
    const auto file = context.files.find(edge.file);
    if (file == context.files.end()) {
      scan.facts.unsupported = true;
      continue;
    }
    const auto path = catalog::filePath(file->second);
    if (!path || !usrs.contains(edge.source) ||
        !usrs.contains(edge.destination)) {
      scan.facts.unsupported = true;
      continue;
    }
    scan.facts.calls[usrs.at(edge.source)].push_back(
        {usrs.at(edge.destination), path->string(), edge.offset, edge.line,
         edge.column, edge.implicit});
  }
}

std::expected<RecoveryBodyFacts, std::string>
scanRecoveryBody(const RecoveryContext &context,
                 const RecoveryCandidate &candidate) {
  const auto found = context.scans.find(candidate.entry.tuFileId);
  if (found == context.scans.end() || found->second->status != 0)
    return std::unexpected("evidence validation compiler failure");
  return found->second->facts;
}
} // namespace facts::commands
