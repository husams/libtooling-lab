#include "commands/analyse/RecoveryEvidenceDefinition.h"
#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "storage/catalog/File.h"

namespace facts::commands {
std::expected<void, std::string>
validateRecoveryDefinition(const RecoveryContext &context,
                           const callgraph::QueryNode &node,
                           const RecoveryBodyFacts &facts) {
  if (!node.definition || !node.definitionLocation)
    return std::unexpected("incomplete persisted body evidence");
  const auto file = context.files.find(node.definitionLocation->file);
  if (file == context.files.end())
    return std::unexpected("definition file is not registered");
  const auto path = catalog::filePath(file->second);
  const auto proof = facts.definitions.find(node.usr);
  if (!path || proof == facts.definitions.end() ||
      std::filesystem::path(proof->second.path).lexically_normal() !=
          path->lexically_normal() ||
      node.definitionLocation->offset != proof->second.offset ||
      node.definitionLocation->size != proof->second.size)
    return std::unexpected("definition extent changed");
  return {};
}

} // namespace facts::commands
