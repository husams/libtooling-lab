#include "commands/analyse/RecoveryPointerEvidence.h"

#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "storage/catalog/File.h"

namespace facts::commands {
std::expected<RecoveryPointerEvidence, std::string>
collectRecoveryPointerEvidence(const RecoveryContext &context,
                               const callgraph::QueryGraph &graph) {
  RecoveryPointerEvidence result;
  std::map<SymbolId, std::string> usrs;
  for (const auto &node : graph.nodes) {
    usrs.emplace(node.id, node.usr);
    result.try_emplace(node.usr);
  }
  for (const auto &call : graph.pointerCalls) {
    const auto source = usrs.find(call.site.source);
    const auto file = context.files.find(call.site.file);
    if (source == usrs.end() || file == context.files.end())
      return std::unexpected("pointer-call source or file is not registered");
    if (call.site.target && (!call.target || call.target->usr.empty()))
      return std::unexpected("pointer-call operand identity is absent");
    const auto path = catalog::filePath(file->second);
    if (!path)
      return std::unexpected(path.error());
    result.at(source->second).insert(
        {call.target ? std::optional{call.target->usr} : std::nullopt,
         path->lexically_normal().string(), call.site.location.offset,
         call.site.location.line, call.site.location.column,
         call.site.signature, call.site.expression});
  }
  return result;
}
} // namespace facts::commands
