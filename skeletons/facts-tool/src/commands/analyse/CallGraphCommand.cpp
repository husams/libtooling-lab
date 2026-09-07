#include "commands/analyse/CallGraphCommand.h"
#include "analysis/callgraph/CallGraphError.h"
#include "commands/analyse/CallGraphCancellation.h"

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphQuery.h"
#include "commands/ConfigurationSupport.h"
#include "commands/analyse/CallGraphRequest.h"
#include "commands/analyse/CallGraphRun.h"

#include <iostream>

namespace facts::commands {
namespace {

std::expected<std::optional<callgraph::CoverageReport>, std::string>
loadOptionalCoverage(const cli::CallGraphOptions &options,
                     const callgraph::QueryGraph &graph) {
  const bool requested =
      !options.configuration.empty() || !options.configurationFile.empty() ||
      config::detail::present("FACTS_TOOL_CONF") ||
      !options.components.empty() || options.callsScope != "all";
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
  CallGraphCancellation cancellation;
  auto result = validateCallGraphRequest(options).and_then([&](auto request) {
    request.cancelled = [&] { return cancellation.cancelled(); };
    return callgraph::loadCallGraph(options.facts)
        .and_then([&](const auto &graph) {
          return loadOptionalCoverage(options, graph)
              .and_then([&](const auto &coverage) {
                const auto *evidence = coverage ? &*coverage : nullptr;
                return makeCallGraphRequest(options, evidence,
                                            request.cancelled)
                    .and_then([&](const auto &) {
                      return runSelectedCallGraph(options, request, graph,
                                                  evidence);
                    });
              });
        });
  });
  if (!result && options.format == "json" &&
      result.error() != "recovery-failed" &&
      !result.error().starts_with("facts-tool: usage error:") &&
      !result.error().starts_with("facts-tool: configuration error:")) {
    std::cout << callgraph::renderCallGraphErrorJson(result.error());
    return 1;
  }
  return result;
}

} // namespace facts::commands
