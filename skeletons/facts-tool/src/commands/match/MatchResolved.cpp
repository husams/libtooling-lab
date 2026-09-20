#include "commands/Match.h"
#include "commands/match/MatchResolved.h"
#include "commands/ConfigurationSupport.h"
#include "commands/ExtractionSetup.h"
#include "commands/match/MatchExecution.h"
#include "commands/match/RelationKinds.h"
#include "storage/FileManager.h"
#include "tooling/StoredCompilationDatabase.h"

namespace facts::commands {
std::expected<match::MatchOutput, std::string>
runMatchResolved(const cli::MatchOptions &configured, bool installSignals,
                  match::BindingPolicy policy) {
  if (configured.traversal && *configured.traversal != "AsIs" &&
      *configured.traversal != "IgnoreUnlessSpelledInSource")
    return std::unexpected("invalid traversal kind");
  if (configured.relationKind) {
    auto kind = match::parseRelationKind(*configured.relationKind);
    if (!kind) return std::unexpected(kind.error());
  }
  auto loaded = loadStoredCompilationDatabase(configured.configuration,
                                              configured.sources);
  if (!loaded)
    return std::unexpected("cannot load project configuration: " + loaded.error());
  auto commands = requireStoredCommands(
      appendExtraArguments(std::move(*loaded), configured.defaultExtraArguments));
  if (!commands)
    return std::unexpected(commands.error());
  auto opened =
      FileManager::openImported(configured.configuration,
                                configured.astCache.enabled, configured.verbosity);
  if (!opened)
    return std::unexpected(opened.error());
  auto registry = requireCompletedRegistry(**opened);
  if (!registry)
    return std::unexpected(registry.error());
  auto sources = selectSources(**commands, configured.sources);
  auto factsDirectory = materializeFactsDirectory(configured.facts);
  if (!factsDirectory)
    return std::unexpected(factsDirectory.error());
  return match::execute(configured, std::move(*commands), **opened, sources,
                       *registry, installSignals, policy);
}

}
