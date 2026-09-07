#include "analysis/callgraph/CallGraphScope.h"

#include <algorithm>
#include <format>
#include <ranges>

namespace facts::callgraph {
namespace {

std::vector<const CoverageComponent *>
namedComponents(const CoverageReport &coverage, std::string_view name) {
  std::vector<const CoverageComponent *> result;
  for (const auto &component : coverage.components)
    if (component.name == name)
      result.push_back(&component);
  return result;
}

std::string candidates(const CoverageReport &coverage) {
  std::vector<std::string> names;
  for (const auto &component : coverage.components)
    if (std::ranges::find(names, component.name) == names.end())
      names.push_back(component.name);
  std::ranges::sort(names);
  std::string result;
  for (const auto &name : names)
    result += (result.empty() ? "" : ", ") + name;
  return result.empty() ? "none" : result;
}

} // namespace

std::expected<ScopeSelection, std::string>
resolveScope(std::vector<std::string> components, CallsScope calls,
             const CoverageReport &coverage) {
  ScopeSelection result{std::move(components), calls};
  for (const auto &name : result.requestedComponents) {
    const auto matches = namedComponents(coverage, name);
    if (matches.empty())
      return std::unexpected(std::format(
          "facts-tool: usage error: unknown component '{}'; candidates: {}",
          name, candidates(coverage)));
    if (matches.size() > 1) {
      std::string paths;
      for (const auto *match : matches)
        paths += (paths.empty() ? "" : ", ") + match->path;
      return std::unexpected(std::format(
          "facts-tool: usage error: ambiguous component '{}'; candidates: {}",
          name, paths));
    }
    result.componentIds.insert(matches.front()->id);
  }
  return result;
}

std::string exclusionReason(const QueryNode &node, const ScopeSelection &scope,
                            const CoverageReport *coverage) {
  if (!scope.active())
    return {};
  const auto *file =
      coverage ? findCoverageEvidenceFile(*coverage, node) : nullptr;
  if (!file)
    return "unclassified";
  if (!scope.componentIds.empty() &&
      !scope.componentIds.contains(file->componentId))
    return "component";
  if (scope.calls == CallsScope::project && file->componentKind != "repo")
    return file->componentKind == "external" ? "calls_scope" : "unclassified";
  if (scope.calls == CallsScope::library && file->componentKind != "external")
    return file->componentKind == "repo" ? "calls_scope" : "unclassified";
  return {};
}

} // namespace facts::callgraph
