#include "commands/analyse/CallGraphRequest.h"

namespace facts::commands {
namespace {
std::unexpected<std::string> usage(std::string message) {
  return std::unexpected("facts-tool: usage error: " + std::move(message));
}
} // namespace

std::expected<CallGraphRequest, std::string>
validateCallGraphRequest(const cli::CallGraphOptions &options) {
  if (options.target && options.all)
    return usage("--to is incompatible with --all");
  if (options.target && options.direction == "callers")
    return usage("--to is incompatible with --direction callers");
  if (options.pathMode && !options.target)
    return usage("--path-mode requires --to");
  if (options.target)
    return CallGraphRequest{callgraph::QueryMode::Path,
                            options.pathMode == "all-simple"
                                ? callgraph::PathMode::AllSimple
                                : callgraph::PathMode::Shortest};
  if (options.direction == "callers")
    return CallGraphRequest{callgraph::QueryMode::Callers};
  return CallGraphRequest{};
}

} // namespace facts::commands
