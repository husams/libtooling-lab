#include "commands/analyse/CallGraphEntryCommand.h"

#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphEntryQuery.h"
#include "analysis/callgraph/CallGraphSelection.h"
#include "analysis/callgraph/CallGraphTraversal.h"
#include "commands/ConfigurationSupport.h"
#include "commands/FactPairValidation.h"

#include <algorithm>
#include <format>
#include <iostream>
#include <optional>
#include <utility>

namespace facts::commands {
namespace {
std::unexpected<std::string> usage(std::string message) {
  return std::unexpected("facts-tool: usage error: " + std::move(message));
}
} // namespace

std::expected<int, std::string>
runCallGraphEntry(const cli::CallGraphEntryOptions &options) {
  if (options.function.empty())
    return usage("--function must not be empty");
  return callgraph::loadCallGraph(options.facts)
      .and_then([&](const auto &graph) {
        auto selected = callgraph::selectOne(graph, options.function, "root");
        if (!selected) {
          auto error = selected.error();
          if (error.starts_with("missing-root:"))
            error.replace(0, std::string("missing-root").size(),
                          "root-not-found");
          return std::expected<int, std::string>{usage(std::move(error))};
        }
        std::optional<callgraph::CoverageReport> coverage;
        auto paired = [&]() -> std::expected<void, std::string> {
          if (options.configuration.empty() &&
              options.configurationFile.empty() &&
              !config::detail::present("FACTS_TOOL_CONF"))
            return {};
          return loadConfiguration(options.configuration,
                                   options.configurationFile, false)
              .and_then([&](const auto &resolved) {
                return validateFactPairForRead(options.facts,
                                               resolved.database.string())
                    .and_then([&] {
                      return callgraph::loadCoverage(resolved.database.string(),
                                                     graph);
                    })
                    .transform(
                        [&](auto report) { coverage = std::move(report); });
              });
        }();
        if (!paired)
          return std::expected<int, std::string>{
              std::unexpected(paired.error())};
        return callgraph::loadCallGraphEntry(options.facts, (*selected)->id)
            .transform([&](auto record) {
              record.leaf =
                  std::none_of(graph.edges.begin(), graph.edges.end(),
                               [&](const auto &edge) {
                                 return edge.source == (*selected)->id;
                               }) &&
                  (*selected)->unresolved == 0;
              if (coverage) {
                const auto traversal = callgraph::traverseCallGraph(
                    graph, {*selected}, std::nullopt, &*coverage);
                record.aggregateCoverage = callgraph::summarizeCoverage(
                    *coverage, graph, traversal.nodes);
              }
              std::cout << (options.format == "json"
                                ? callgraph::renderCallGraphEntryJson(
                                      **selected, record,
                                      coverage ? &*coverage : nullptr)
                                : callgraph::renderCallGraphEntryText(
                                      **selected, record,
                                      coverage ? &*coverage : nullptr));
              return 0;
            });
      });
}
} // namespace facts::commands
