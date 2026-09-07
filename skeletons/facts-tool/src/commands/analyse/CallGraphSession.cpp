#include "commands/analyse/CallGraphSession.h"
#include "commands/ConfigurationSupport.h"
#include <filesystem>

namespace facts::commands {
namespace {
bool samePath(const std::string &a, const std::string &b) {
  if (a.empty() || b.empty())
    return false;
  std::error_code error;
  if (std::filesystem::equivalent(a, b, error) && !error)
    return true;
  error.clear();
  auto left = std::filesystem::weakly_canonical(a, error);
  if (error)
    return false;
  auto right = std::filesystem::weakly_canonical(b, error);
  return !error && left == right;
}
} // namespace

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

std::expected<void, std::string>
validateGraphOutput(const cli::CallGraphOptions &options,
                    const callgraph::CoverageReport *coverage) {
  if (options.output.empty())
    return {};
  bool overlaps = samePath(options.output, options.facts) ||
                  samePath(options.output, options.configuration) ||
                  samePath(options.output, options.configurationFile);
  if (coverage)
    for (const auto &file : coverage->files)
      overlaps |= samePath(options.output, file.path);
  if (overlaps)
    return std::unexpected(
        "facts-tool: usage error: graph output overlaps an input");
  return {};
}
} // namespace facts::commands
