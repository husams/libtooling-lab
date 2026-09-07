#include "commands/analyse/CallGraphRecoverySelection.h"
#include "commands/analyse/RecoveryReuseReport.h"
#include <algorithm>

namespace facts::commands {
std::vector<RecoveryEntry>
collectRecoveryReuseReport(RecoveryContext &context,
                           const callgraph::QueryGraph &graph) {
  RecoveryReuseGroups groups;
  for (const auto &node : graph.nodes) {
    if (!context.reusedUsrs.contains(node.usr))
      continue;
    if (const auto owner = context.reusedOwners.find(node.usr);
        owner != context.reusedOwners.end()) {
      groups[owner->second].insert(node.usr);
      continue;
    }
    const auto file =
        node.definitionLocation ? node.definitionLocation->file : node.id.file;
    const auto registered = context.files.find(file);
    if (registered == context.files.end() ||
        !registered->second.component.repositoryId)
      continue;
    if (context.commands.contains(file)) {
      groups[file].insert(node.usr);
      continue;
    }
    for (const auto &[id, command] : context.commands) {
      RecoveryCandidate candidate{makeEntry(context, id, {}, ""), command.path};
      const auto inputs = recoveryRegisteredInputs(context, candidate);
      if (context.inputClosures.contains(id) &&
          std::ranges::any_of(inputs, [&](const auto &input) {
            return input.file_id == file;
          })) {
        groups[id].insert(node.usr);
        break;
      }
    }
  }
  return makeRecoveryReuseReport(context, groups);
}
} // namespace facts::commands
