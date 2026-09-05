#pragma once

#include "config/Configuration.h"
#include <optional>

namespace facts::config::detail {

// The three YAML tiers that can contribute to one effective configuration,
// highest scalar precedence first: an explicit --config/FACTS_TOOL_CONFIG
// file, the project file, and the user file.
struct MergeContext {
  std::optional<Tier> configFile;
  std::optional<Tier> project;
  std::optional<Tier> user;
};

// conf_root/conf_template/facts_template take the highest-precedence tier
// that sets them (configFile > project > user > built-in default already in
// base). extra_args is a fixed-order concatenation: user, then project, then
// configFile, regardless of which tier "wins" for the scalar keys.
Resolved mergeTiers(Resolved base, const MergeContext &context);

} // namespace facts::config::detail
