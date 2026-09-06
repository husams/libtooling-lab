#include "commands/analyse/CallGraphCommand.h"

#include "analysis/callgraph/CallGraphCoverage.h"
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
  return callgraph::loadCallGraph(options.facts)
      .and_then([&](const auto &graph) {
        if (graph.edges.empty())
          return std::expected<int, std::string>{
              std::unexpected("facts database contains no call facts")};
        return loadOptionalCoverage(options, graph)
            .and_then([&](const auto &coverage) {
              return callgraph::selectRoots(graph, options.function,
                                            options.all)
                  .transform([&](const auto &roots) {
                    const auto *evidence = coverage ? &*coverage : nullptr;
                    const auto view = options.edges == "calls"
                                          ? callgraph::EdgeView::Calls
                                          : callgraph::EdgeView::Semantic;
                    const auto traversal = callgraph::renderCallGraph(
                        graph, roots, options.maxDepth, evidence, view);
                    std::cout
                        << (options.format == "json"
                                ? callgraph::renderCallGraphJson(
                                      graph, roots, traversal, evidence, view)
                                : traversal.text);
                    return 0;
                  });
            });
      });
}

} // namespace facts::commands
