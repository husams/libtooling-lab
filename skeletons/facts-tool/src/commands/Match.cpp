#include "commands/Match.h"
#include "commands/match/MatchResolved.h"

#include "commands/CompilationDatabase.h"
#include "commands/ConfigurationSupport.h"
#include "commands/DatabasePaths.h"
#include "commands/ExtractionSetup.h"
#include "commands/FactConfiguration.h"
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
  if (configured.factsProvided && configured.facts.empty())
    return std::unexpected("facts-tool: usage error: --facts must not be empty");
  auto session = loadFactConfiguration(configured.configuration,
                                       configured.configurationFile,
                                       configured.facts, configured.sources,
                                       true);
  if (!session)
    return std::unexpected(session.error());
  const bool combined = configured.factsProvided && !session->projectSelected;
  configured.facts = session->facts.string();
  configured.configuration = combined ? configured.facts
                                       : session->project.database.string();
  configured.astCache = session->project.astCache;
  configured.astCache.database = configured.configuration;
  configured.astCache.verbosity = options.verbosity;
  if (!combined) {
    auto paths =
        validateDatabasePaths(configured.facts, configured.configuration);
    if (!paths)
      return std::unexpected("facts-tool: configuration error: " +
                             paths.error());
  }
  if (!combined && std::filesystem::exists(configured.facts)) {
    auto pairing =
        validateFactPairForRead(configured.facts, configured.configuration);
    if (!pairing)
      return std::unexpected(pairing.error());
  }
  configured.defaultExtraArguments = session->project.extraArguments;
  return runMatchResolved(configured).transform([&](match::MatchOutput output) {
    match::writeResults(std::move(output), configured.format == "json", std::cout);
    return 0;
  });
}


} // namespace facts::commands
