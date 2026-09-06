#include "commands/analyse/CallGraphCommand.h"
#include "commands/analyse/CallGraphCancellation.h"
#include "commands/analyse/CallGraphRequest.h"

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphError.h"
#include "analysis/callgraph/CallGraphJson.h"
#include "analysis/callgraph/CallGraphQuery.h"
#include "analysis/callgraph/CallGraphTraversal.h"
#include "commands/ConfigurationSupport.h"

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
  auto result =
      callgraph::loadCallGraph(options.facts).and_then([&](const auto &graph) {
        if (graph.edges.empty())
          return std::expected<int, std::string>{
              std::unexpected("facts database contains no call facts")};
        return loadOptionalCoverage(options, graph)
            .and_then([&](const auto &coverage) {
              return callgraph::selectRoots(graph, options.function,
                                            options.all)
                  .transform_error([](const auto &reason) {
                    return "facts-tool: usage error: " + reason;
                  })
                  .and_then([&](const auto &roots)
                                -> std::expected<int, std::string> {
                    const auto *evidence = coverage ? &*coverage : nullptr;
                    auto request = makeCallGraphRequest(options, evidence, [&] {
                      return cancellation.cancelled();
                    });
                    if (!request)
                      return std::unexpected(request.error());
                    const auto traversal = callgraph::renderCallGraph(
                        graph, roots, std::move(*request), evidence);
                    std::cout << (options.format == "json"
                                      ? callgraph::renderCallGraphJson(
                                            graph, roots, traversal, evidence)
                                      : traversal.text);
                    return traversal.reason == "cancelled" ? 130 : 0;
                  });
            });
      });
  if (!result && options.format == "json" &&
      !result.error().starts_with("facts-tool: usage error:") &&
      !result.error().starts_with("facts-tool: configuration error:")) {
    std::cout << callgraph::renderCallGraphErrorJson(result.error());
    return 1;
  }
  return result;
}

} // namespace facts::commands
