#include "commands/analyse/CallGraphCommand.h"
#include "analysis/callgraph/CallGraphError.h"
#include "commands/analyse/CallGraphCancellation.h"
#include "commands/analyse/CallGraphOutput.h"
#include "commands/analyse/CallGraphRequest.h"
#include "commands/analyse/CallGraphRun.h"
#include "commands/analyse/CallGraphSession.h"

namespace facts::commands {
namespace {
std::expected<int, std::string>
runResolvedGraph(const cli::CallGraphOptions &options,
                 const CallGraphRequest &request) {
  return callgraph::loadCallGraph(options.facts)
      .and_then([&](const auto &graph) -> std::expected<int, std::string> {
        std::optional<callgraph::CoverageReport> coverage;
        if (!options.configuration.empty()) {
          auto loaded = callgraph::loadCoverage(options.configuration, graph);
          if (!loaded)
            return std::unexpected(loaded.error());
          coverage = std::move(*loaded);
        }
        const auto *evidence = coverage ? &*coverage : nullptr;
        return validateGraphOutput(options, evidence).and_then([&] {
          return runSelectedCallGraph(options, request, graph, evidence);
        });
      });
}
} // namespace

std::expected<int, std::string>
runCallGraph(const cli::CallGraphOptions &options) {
  CallGraphCancellation cancellation;
  auto result =
      validateCallGraphRequest(options).and_then([&](const auto &request) {
        return resolveGraphSession(options).and_then([&](const auto &resolved) {
          return runResolvedGraph(resolved, request);
        });
      });
  if (!result && options.format == "json" && options.output.empty() &&
      result.error() != "recovery-failed" &&
      !result.error().starts_with("facts-tool: usage error:") &&
      !result.error().starts_with("facts-tool: configuration error:")) {
    return writeGraphOutput({},
                            callgraph::renderCallGraphErrorJson(result.error()))
        .transform([] { return 1; });
  }
  return result;
}
} // namespace facts::commands
