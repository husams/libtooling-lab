#include "commands/Match.h"

#include "commands/ConfigurationSupport.h"
#include "commands/DatabasePaths.h"
#include "commands/ExtractionSetup.h"
#include "commands/FactPairValidation.h"
#include "commands/match/MatchExecution.h"
#include "commands/match/RelationKinds.h"
#include "storage/FileManager.h"
#include "tooling/StoredCompilationDatabase.h"

namespace facts::commands {
namespace {} // namespace

std::expected<int, std::string> runMatch(const cli::MatchOptions &options) {
  if (options.relationKind) {
    auto kind = match::parseRelationKind(*options.relationKind);
    if (!kind)
      return std::unexpected(kind.error());
  }
  auto configured = options;
  configured.sources = normalizeSourceSelectors(options.sources);
  const bool explicitConfiguration = !options.configuration.empty() ||
                                     !options.configurationFile.empty() ||
                                     config::detail::present("FACTS_TOOL_CONF");
  if (configured.factsProvided && !explicitConfiguration) {
    // A supplied --facts path historically names the combined imported
    // project/facts database; preserve that contract when --conf is omitted.
    configured.configuration = configured.facts;
  } else {
    auto resolved = loadConfiguration(options.configuration,
                                      options.configurationFile, false, true);
    if (!resolved)
      return std::unexpected(resolved.error());
    configured.configuration = resolved->database.string();
    if (!configured.factsProvided) {
      auto facts = resolveFactsOutput(*resolved, configured.sources);
      if (!facts)
        return std::unexpected(facts.error());
      configured.facts = facts->string();
    }
  }
  if (configured.facts.empty())
    return std::unexpected(
        "facts-tool: usage error: --facts must not be empty");
  if (!(configured.factsProvided && !explicitConfiguration)) {
    auto paths =
        validateDatabasePaths(configured.facts, configured.configuration);
    if (!paths)
      return std::unexpected("facts-tool: configuration error: " +
                             paths.error());
  }
  if (!(configured.factsProvided && !explicitConfiguration) &&
      std::filesystem::exists(configured.facts)) {
    auto pairing =
        validateFactPairForRead(configured.facts, configured.configuration);
    if (!pairing)
      return std::unexpected(pairing.error());
  }
  auto loaded = loadStoredCompilationDatabase(configured.configuration,
                                              configured.sources);
  if (!loaded)
    return std::unexpected(
        "cannot load project configuration: " + loaded.error() +
        (configured.factsProvided && !explicitConfiguration
             ? "; pass --conf <project db> for a separate project database, "
               "or omit --facts to use facts_template"
             : ""));
  auto commands = requireStoredCommands(std::move(*loaded));
  if (!commands)
    return std::unexpected(commands.error());
  auto opened =
      FileManager::openReadOnly(configured.configuration, configured.verbosity);
  if (!opened)
    return std::unexpected(opened.error());
  auto registry = requireCompletedRegistry(**opened);
  if (!registry)
    return std::unexpected(registry.error());
  auto sources = selectSources(**commands, configured.sources);
  auto registered =
      requireRegisteredSources(**opened, **commands, sources, *registry);
  if (!registered)
    return std::unexpected(registered.error());
  auto factsDirectory = materializeFactsDirectory(configured.facts);
  if (!factsDirectory)
    return std::unexpected(factsDirectory.error());
  return match::execute(configured, std::move(*commands), **opened, sources);
}

} // namespace facts::commands
