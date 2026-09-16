#pragma once

#include "config/ConfigurationMerge.h"

namespace facts::config::detail {

void mergeAstCache(Resolved &value, const MergeContext &context);

// Resolve directory syntax without creating or opening the cache.
std::expected<Resolved, std::string> resolveAstCache(Resolved value);

} // namespace facts::config::detail
