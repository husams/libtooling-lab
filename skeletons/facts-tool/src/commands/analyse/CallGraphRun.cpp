#include "commands/analyse/CallGraphRun.h"
#include "analysis/callgraph/CallGraphSelection.h"
#include "cli/Verbose.h"
#include "commands/analyse/CallGraphCancellation.h"
#include "commands/analyse/CallGraphOutcome.h"
#include "commands/analyse/CallGraphResult.h"

namespace facts::commands {
namespace {
std::unexpected<std::string> usage(const std::string &message) {
  return std::unexpected("facts-tool: usage error: " + message);
}
} // namespace

std::expected<void, std::string>
validateCallGraphSelection(const cli::CallGraphOptions &options,
                           const CallGraphRequest &request,
                           const callgraph::QueryGraph &graph,
                           const callgraph::CoverageReport *coverage) {
  auto roots = callgraph::selectRoots(graph, options.function, options.all);
  if (!roots)
    return usage(roots.error());
  if (request.mode == callgraph::QueryMode::Path) {
    auto target = callgraph::selectOne(graph, *options.target, "target");
    if (!target)
      return usage(target.error());
  }
  auto controls = makeCallGraphRequest(options, coverage, {});
  if (!controls)
    return std::unexpected(controls.error());
  cli::logVerbose(options.verbosity, 1, "facts-tool: roots selected");
  for (const auto *root : *roots)
    cli::logVerbose(options.verbosity, 2, "facts-tool: root name='{}' usr='{}'",
                    root->name, root->usr);
  return {};
}

std::expected<CallGraphResult, std::string>
queryCallGraph(const cli::CallGraphOptions &options,
               const CallGraphRequest &request,
               const callgraph::QueryGraph &graph,
               const callgraph::CoverageReport *coverage) {
  auto roots = callgraph::selectRoots(graph, options.function, options.all);
  if (!roots)
    return usage(roots.error());
  cli::logVerbose(options.verbosity, 1, "facts-tool: graph traversal");
  auto controls = makeCallGraphRequest(
      options, coverage, [&] { return CallGraphCancellation::cancelled(); });
  if (!controls)
    return std::unexpected(controls.error());
  CallGraphResult result;
  result.roots = *roots;
  if (request.mode == callgraph::QueryMode::Callees) {
    result.traversal =
        callgraph::traverseCallGraph(graph, *roots, *controls, coverage);
  } else if (request.mode == callgraph::QueryMode::Callers) {
    result.traversal =
        callgraph::searchCallersWithRequest(graph, *roots, *controls, coverage);
  } else {
    auto target = callgraph::selectOne(graph, *options.target, "target");
    if (!target)
      return usage(target.error());
    result.target = *target;
    auto search = callgraph::searchPathsWithRequest(graph, *roots->front(),
                                                    **target, request.pathMode,
                                                    *controls, coverage);
    result.traversal = std::move(search.traversal);
    result.paths = std::move(search.paths);
  }
  return result;
}

std::expected<CallGraphRunRecord, std::string>
runStoredCallGraph(const cli::CallGraphOptions &options,
                   const CallGraphRequest &request,
                   const callgraph::QueryGraph &graph,
                   const callgraph::CoverageReport *coverage) {
  return queryCallGraph(options, request, graph, coverage)
      .transform([&](const auto &result) {
        return makeCallGraphRunRecord(options, request, result, nullptr,
                                      CallGraphCancellation::cancelled(), {});
      });
}
} // namespace facts::commands
