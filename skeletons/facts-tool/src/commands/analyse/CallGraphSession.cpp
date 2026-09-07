#include "commands/analyse/CallGraphSession.h"
#include "commands/ConfigurationSupport.h"
#include <filesystem>

namespace facts::commands {
std::expected<cli::CallGraphOptions, std::string>
resolveGraphSession(cli::CallGraphOptions options) {
  const bool configured = !options.configuration.empty() ||
                          !options.configurationFile.empty() ||
                          config::detail::present("FACTS_TOOL_CONF");
  if (configured || options.facts.empty()) {
    auto resolved =
        loadConfiguration(options.configuration, options.configurationFile,
                          false, options.facts.empty());
    if (!resolved)
      return std::unexpected(resolved.error());
    options.configuration = resolved->database.string();
    if (options.facts.empty()) {
      auto facts = resolveFactsOutput(*resolved, {});
      if (!facts)
        return std::unexpected(facts.error());
      options.facts = facts->string();
    }
  }
  std::error_code error;
  auto facts = std::filesystem::weakly_canonical(options.facts, error);
  if (error)
    return std::unexpected("facts-tool: configuration error: " +
                           error.message());
  options.facts = facts.string();
  return options;
}
} // namespace facts::commands
