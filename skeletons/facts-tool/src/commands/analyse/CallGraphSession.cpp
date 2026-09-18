#include "commands/analyse/CallGraphSession.h"
#include "commands/FactConfiguration.h"
#include <filesystem>

namespace facts::commands {
std::expected<cli::CallGraphOptions, std::string>
resolveGraphSession(cli::CallGraphOptions options) {
  auto resolved = loadFactConfiguration(
      options.configuration, options.configurationFile, options.facts, {},
      options.recoverMissing);
  if (!resolved)
    return std::unexpected(resolved.error());
  options.configuration = resolved->projectSelected
                              ? resolved->project.database.string()
                              : "";
  options.astCache = resolved->project.astCache;
  options.facts = resolved->facts.string();
  options.astCache.verbosity = options.verbosity;
  std::error_code error;
  auto facts = std::filesystem::weakly_canonical(options.facts, error);
  if (error)
    return std::unexpected("facts-tool: configuration error: " +
                           error.message());
  options.facts = facts.string();
  return options;
}
} // namespace facts::commands
