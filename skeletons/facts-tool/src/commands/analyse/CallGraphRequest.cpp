#include "commands/analyse/CallGraphRequest.h"

#include "analysis/callgraph/CallGraphScope.h"

#include <limits>

namespace facts::commands {
namespace {
std::expected<callgraph::CallsScope, std::string>
parseScope(std::string_view value) {
  if (value == "all")
    return callgraph::CallsScope::all;
  if (value == "project")
    return callgraph::CallsScope::project;
  if (value == "library")
    return callgraph::CallsScope::library;
  return std::unexpected("facts-tool: usage error: invalid --calls-scope");
}
} // namespace

std::expected<callgraph::TraversalRequest, std::string>
makeCallGraphRequest(const cli::CallGraphOptions &options,
                     const callgraph::CoverageReport *coverage,
                     std::function<bool()> cancelled) {
  return parseScope(options.callsScope)
      .and_then([&](auto calls)
                    -> std::expected<callgraph::TraversalRequest, std::string> {
        if ((!options.components.empty() ||
             calls != callgraph::CallsScope::all) &&
            !coverage)
          return std::unexpected(
              "facts-tool: configuration error: graph scope requires a "
              "project/facts pair");
        const auto maximum = static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max());
        if (options.timeLimitMs && *options.timeLimitMs > maximum)
          return std::unexpected("facts-tool: usage error: --time-limit-ms "
                                 "overflows milliseconds");
        auto scope =
            coverage
                ? callgraph::resolveScope(options.components, calls, *coverage)
                : std::expected<callgraph::ScopeSelection, std::string>{
                      callgraph::ScopeSelection{{}, calls}};
        return scope.transform([&](auto resolved) {
          callgraph::TraversalLimits limits{options.maxDepth, options.maxNodes,
                                            options.maxEdges, std::nullopt};
          if (options.timeLimitMs)
            limits.time = std::chrono::milliseconds(*options.timeLimitMs);
          return callgraph::TraversalRequest{std::move(resolved), limits,
                                             std::move(cancelled)};
        });
      });
}

} // namespace facts::commands
