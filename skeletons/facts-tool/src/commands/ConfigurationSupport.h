#pragma once

#include "commands/ExtraArguments.h"
#include "config/Configuration.h"
#include "config/ConfigurationDiscovery.h"

namespace facts::commands {
inline std::expected<config::Resolved, std::string>
loadConfiguration(std::string_view direct, std::string_view selector,
                  bool create, bool compilerDefaults = false) {
  const bool directOverride = !direct.empty() ||
                              config::detail::present("FACTS_TOOL_CONF");
  auto result = config::resolve({std::string(selector), std::string(direct),
                                 create, compilerDefaults || !directOverride});
  if (!result) return std::unexpected("facts-tool: configuration error: " + result.error());
  if (create && result->generated) {
    if (auto owned = config::ensureOwnedDatabase(*result); !owned)
      return std::unexpected("facts-tool: configuration error: " +
                             owned.error());
  }
  return result;
}
inline std::expected<std::vector<std::string>, std::string>
mergedArguments(const std::vector<std::string> &defaults,
                const std::vector<std::string> &explicitValues) {
  return tokenizeExtraArguments(explicitValues).transform([&](auto values) {
    std::vector<std::string> result = defaults;
    result.insert(result.end(), values.begin(), values.end());
    return result;
  });
}

// facts_template supplies the default -o/--facts path when the caller left
// it empty. Errors are fully prefixed so commands can return them as-is.
inline std::expected<std::filesystem::path, std::string>
resolveFactsOutput(const config::Resolved &resolved,
                   const std::vector<std::string> &sources) {
  return config::renderFactsPath(resolved, sources).transform_error([](std::string reason) {
    return reason.starts_with("usage:") ? "facts-tool: usage error: " + reason.substr(6)
                                        : "facts-tool: configuration error: " + reason;
  });
}

// Extract and dependency analysis write the facts database, so once the
// path is resolved they materialize its parent directory; symbol commands
// are read-only and must call resolveFactsOutput directly instead.
inline std::expected<std::filesystem::path, std::string>
resolveWritableFactsOutput(const config::Resolved &resolved,
                          const std::vector<std::string> &sources) {
  return resolveFactsOutput(resolved, sources)
      .and_then([](std::filesystem::path path)
                    -> std::expected<std::filesystem::path, std::string> {
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
          return std::unexpected(
              "facts-tool: configuration error: cannot create facts_template "
              "directory: " +
              error.message());
        return path;
      });
}
}
