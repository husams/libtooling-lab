#include "commands/Match.h"
#include "commands/match/MatchResolved.h"

#include "commands/CompilationDatabase.h"
#include "commands/ConfigurationSupport.h"
#include "commands/DatabasePaths.h"
#include "commands/ExtractionSetup.h"
#include "commands/FactPairValidation.h"
#include "commands/match/MatchExecution.h"
#include "commands/match/MatchResult.h"
#include <iostream>
#include "commands/match/RelationKinds.h"
#include "storage/FileManager.h"
#include "tooling/StoredCompilationDatabase.h"

namespace facts::commands {
namespace {

bool validTraversal(const std::optional<std::string> &traversal) {
  return !traversal || *traversal == "AsIs" ||
         *traversal == "IgnoreUnlessSpelledInSource";
}

} // namespace

std::expected<int, std::string> runMatch(const cli::MatchOptions &options) {
  if (!validTraversal(options.traversal))
    return std::unexpected(
        "facts-tool: usage error: --traversal must be AsIs or "
        "IgnoreUnlessSpelledInSource");
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
  }
  auto resolved = loadConfiguration(configured.configuration,
                                    configured.configurationFile, false, true);
  if (!resolved)
    return std::unexpected(resolved.error());
  configured.configuration = resolved->database.string();
  if (configured.factsProvided && !explicitConfiguration)
    configured.facts = configured.configuration;
  configured.astCache = resolved->astCache;
  configured.astCache.verbosity = options.verbosity;
  if (!configured.factsProvided) {
    auto facts = resolveFactsOutput(*resolved, configured.sources);
    if (!facts)
      return std::unexpected(facts.error());
    configured.facts = facts->string();
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
  configured.defaultExtraArguments = resolved->extraArguments;
  return runMatchResolved(configured).transform([&](match::MatchOutput output) {
    match::writeResults(std::move(output), configured.format == "json", std::cout);
    return 0;
  });
}


} // namespace facts::commands
