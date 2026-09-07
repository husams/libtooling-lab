#include "commands/analyse/CallGraphRun.h"
#include "analysis/callgraph/CallGraphSelection.h"
#include "analysis/callgraph/CallGraphText.h"
#include "cli/Trace.h"
#include "commands/analyse/CallGraphCancellation.h"
#include "commands/analyse/CallGraphResult.h"
#include <format>

namespace facts::commands {
std::expected<CallGraphResult, std::string>
queryCallGraph(const cli::CallGraphOptions &options,
               const CallGraphRequest &request,
               const callgraph::QueryGraph &graph,
               const callgraph::CoverageReport *coverage) {
  auto roots = callgraph::selectRoots(graph, options.function, options.all);
  if (!roots)
    return std::unexpected("facts-tool: usage error: " + roots.error());
  cli::logVerbose(options.verbosity, 1, "facts-tool: graph traversal");
  auto controls = makeCallGraphRequest(
      options, coverage, [&] { return CallGraphCancellation::cancelled(); });
  if (!controls)
    return std::unexpected(controls.error());
  const auto view = options.edges == "calls" ? callgraph::EdgeView::Calls
                                             : callgraph::EdgeView::Semantic;
  CallGraphResult result;
  result.roots = *roots;
  if (request.mode == callgraph::QueryMode::Callees) {
    result.traversal =
        callgraph::renderCallGraph(graph, *roots, *controls, coverage, view);
  } else if (request.mode == callgraph::QueryMode::Callers) {
    result.traversal =
        callgraph::searchCallersWithRequest(graph, *roots, *controls, coverage);
    result.traversal.text =
        "query=callers\n" + callgraph::renderCallGraphText(graph, *roots,
                                                           result.traversal,
                                                           coverage, view);
  } else {
    auto target = callgraph::selectOne(graph, *options.target, "target");
    if (!target)
      return std::unexpected("facts-tool: usage error: " + target.error());
    result.target = *target;
    auto search = callgraph::searchPathsWithRequest(graph, *roots->front(),
                                                    **target, request.pathMode,
                                                    *controls, coverage);
    result.pathResult = callgraph::pathResult(graph, search, coverage);
    result.traversal = std::move(search.traversal);
    result.paths = std::move(search.paths);
    result.traversal.text = std::format(
        "query=path path-mode={} path-result={}\n{}",
        callgraph::pathModeName(request.pathMode), result.pathResult,
        callgraph::renderCallGraphText(graph, *roots, result.traversal,
                                       coverage, view));
  }
  return result;
}

std::expected<int, std::string>
runCallGraphQuery(const cli::CallGraphOptions &options,
                  const CallGraphRequest &request,
                  const callgraph::QueryGraph &graph,
                  const callgraph::CoverageReport *coverage,
                  const callgraph::RecoveryReport *recovery) {
  return queryCallGraph(options, request, graph, coverage)
      .and_then([&](const auto &result) {
        return publishCallGraph(options, request, graph, coverage, result,
                                recovery);
      });
}
} // namespace facts::commands
