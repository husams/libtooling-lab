#pragma once

#include "commands/ExtraArguments.h"
#include "config/Configuration.h"
#include "config/ConfigurationDiscovery.h"

namespace facts::commands {
inline std::expected<config::Resolved, std::string>
loadConfiguration(std::string_view direct, std::string_view selector,
                  bool create) {
  const bool directOverride = !direct.empty() ||
                              config::detail::present("FACTS_TOOL_CONF");
  auto result = config::resolve({std::string(selector), std::string(direct),
                                 create, !directOverride});
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
}
