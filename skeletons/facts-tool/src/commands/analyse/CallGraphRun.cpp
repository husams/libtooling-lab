#include "commands/analyse/CallGraphRun.h"

#include "analysis/callgraph/CallGraphJson.h"
#include "analysis/callgraph/CallGraphSearch.h"
#include "analysis/callgraph/CallGraphSelection.h"
#include "analysis/callgraph/CallGraphText.h"
#include "commands/analyse/CallGraphRecovery.h"

#include <format>
#include <iostream>

namespace facts::commands {
namespace {
std::unexpected<std::string> usage(std::string message) {
  return std::unexpected("facts-tool: usage error: " + std::move(message));
}

callgraph::EdgeView edgeView(const cli::CallGraphOptions &options) {
  return options.edges == "calls" ? callgraph::EdgeView::Calls
                                  : callgraph::EdgeView::Semantic;
}

void printOutput(const cli::CallGraphOptions &options,
                 const callgraph::QueryGraph &graph,
                 const std::vector<const callgraph::QueryNode *> &roots,
                 const callgraph::RenderedGraph &traversal,
                 const callgraph::CoverageReport *coverage,
                 callgraph::EdgeView view, callgraph::QueryMode mode,
                 std::optional<callgraph::PathMode> pathMode = std::nullopt,
                 const callgraph::QueryNode *target = nullptr,
                 const std::vector<callgraph::QueryPath> &paths = {},
                 std::string_view result = {},
                 const callgraph::RecoveryReport *recovery = nullptr) {
  std::cout << (options.format == "json"
                    ? callgraph::renderCallGraphJson(
                          graph, roots, traversal, coverage, view, mode,
                          pathMode, target, paths, result, recovery)
                    : traversal.text);
}
} // namespace

std::expected<int, std::string>
runCallGraphQuery(const cli::CallGraphOptions &options,
                     const CallGraphRequest &request,
                     const callgraph::QueryGraph &graph,
                     const callgraph::CoverageReport *coverage,
                     const callgraph::RecoveryReport *recovery) {
  auto roots = callgraph::selectRoots(graph, options.function, options.all);
  if (!roots)
    return usage(roots.error());
  const auto view = edgeView(options);
  if (request.mode == callgraph::QueryMode::Callees) {
    if (graph.edges.empty() && !recovery)
      return std::unexpected("facts database contains no call facts");
    const auto traversal = callgraph::renderCallGraph(
        graph, *roots, options.maxDepth, coverage, view);
    printOutput(options, graph, *roots, traversal, coverage, view,
                request.mode, std::nullopt, nullptr, {}, {},
                recovery);
    return 0;
  }
  if (request.mode == callgraph::QueryMode::Callers) {
    auto traversal =
        callgraph::searchCallers(graph, *roots, options.maxDepth,
                                 coverage);
    traversal.text = "query=callers\n" +
                     callgraph::renderCallGraphText(graph, *roots,
                                                    traversal, coverage,
                                                    view);
    printOutput(options, graph, *roots, traversal, coverage, view,
                request.mode, std::nullopt, nullptr, {}, {},
                recovery);
    return 0;
  }
  auto target = callgraph::selectOne(graph, *options.target, "target");
  if (!target)
    return usage(target.error());
  auto search =
      callgraph::searchPaths(graph, *roots->front(), **target,
                             request.pathMode, options.maxDepth,
                             coverage);
  const auto result = callgraph::pathResult(graph, search,
                                            coverage);
  search.traversal.text =
      std::format("query=path path-mode={} path-result={}\n{}",
                  callgraph::pathModeName(request.pathMode), result,
                  callgraph::renderCallGraphText(
                      graph, *roots, search.traversal,
                      coverage, view));
  printOutput(options, graph, *roots, search.traversal, coverage,
              view, request.mode, request.pathMode, *target, search.paths,
              result, recovery);
  return 0;
}

} // namespace facts::commands
