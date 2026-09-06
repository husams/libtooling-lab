#include "commands/analyse/CallGraphCommand.h"

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphQuery.h"
#include "commands/ConfigurationSupport.h"
#include "commands/analyse/CallGraphRequest.h"
#include "commands/analyse/CallGraphRun.h"

namespace facts::commands {
namespace {

std::expected<std::optional<callgraph::CoverageReport>, std::string>
loadOptionalCoverage(const cli::CallGraphOptions &options,
                     const callgraph::QueryGraph &graph) {
  const bool requested = !options.configuration.empty() ||
                         !options.configurationFile.empty() ||
                         config::detail::present("FACTS_TOOL_CONF");
  if (!requested)
    return std::optional<callgraph::CoverageReport>{};
  return loadConfiguration(options.configuration, options.configurationFile,
                           false)
      .and_then([&](const auto &resolved) {
        return callgraph::loadCoverage(resolved.database.string(), graph);
      })
      .transform([](auto report) {
        return std::optional<callgraph::CoverageReport>{std::move(report)};
      });
}

} // namespace

std::expected<int, std::string>
runCallGraph(const cli::CallGraphOptions &options) {
  return validateCallGraphRequest(options).and_then([&](const auto &request) {
    return callgraph::loadCallGraph(options.facts)
        .and_then([&](const auto &graph) {
          return loadOptionalCoverage(options, graph)
              .and_then([&](const auto &coverage) {
                const auto *evidence = coverage ? &*coverage : nullptr;
                return runSelectedCallGraph(options, request, graph, evidence);
              });
        });
  });
}

} // namespace facts::commands
