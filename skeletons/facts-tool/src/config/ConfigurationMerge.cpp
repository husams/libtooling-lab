#include "config/ConfigurationMerge.h"

#include <array>

namespace facts::config::detail {
namespace {

using Tiers = std::array<const std::optional<Tier> *, 3>;

Tiers scalarPriority(const MergeContext &context) {
  return {&context.configFile, &context.project, &context.user};
}

void mergeConfRoot(Resolved &base, const MergeContext &context) {
  for (const auto *tier : scalarPriority(context)) {
    if (*tier && (**tier).confRoot) {
      base.storageRoot = *(**tier).confRoot;
      base.storageRootSource = (**tier).path.string();
      return;
    }
  }
}

void mergeConfTemplate(Resolved &base, const MergeContext &context) {
  for (const auto *tier : scalarPriority(context)) {
    if (*tier && (**tier).confTemplate) {
      base.templateText = *(**tier).confTemplate;
      base.templateSource = (**tier).path.string();
      return;
    }
  }
}

void mergeFactsTemplate(Resolved &base, const MergeContext &context) {
  for (const auto *tier : scalarPriority(context)) {
    if (*tier && (**tier).factsTemplate) {
      base.factsTemplate = *(**tier).factsTemplate;
      base.factsTemplateSource = (**tier).path.string();
      return;
    }
  }
}

// Fixed concatenation order regardless of scalar precedence: least specific
// (user) first, most specific (project) next, then an explicit --config file.
void mergeExtraArgs(Resolved &base, const MergeContext &context) {
  std::vector<std::string> sources;
  for (const auto *tier : {&context.user, &context.project, &context.configFile}) {
    if (!*tier || !(**tier).extraArgs) continue;
    const auto &values = *(**tier).extraArgs;
    base.extraArguments.insert(base.extraArguments.end(), values.begin(), values.end());
    sources.push_back((**tier).path.string());
  }
  if (!sources.empty()) {
    base.extraArgumentsSource.clear();
    for (const auto &source : sources)
      base.extraArgumentsSource += (base.extraArgumentsSource.empty() ? "" : ", ") + source;
  }
}

} // namespace

Resolved mergeTiers(Resolved base, const MergeContext &context) {
  mergeConfRoot(base, context);
  mergeConfTemplate(base, context);
  mergeFactsTemplate(base, context);
  mergeExtraArgs(base, context);
  return base;
}

} // namespace facts::config::detail
