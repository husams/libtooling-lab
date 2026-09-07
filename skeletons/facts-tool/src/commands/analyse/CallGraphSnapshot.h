#pragma once

#include "commands/analyse/CallGraphRecovery.h"
#include "commands/analyse/CallGraphResult.h"
#include <memory>

namespace facts::commands {
// Results contain pointers into the graph, so retain its owning generation.
struct CallGraphSnapshot {
  callgraph::QueryGraph graph;
  std::optional<callgraph::CoverageReport> coverage;
  CallGraphResult result;

  const callgraph::CoverageReport *evidence() const {
    return coverage ? &*coverage : nullptr;
  }
};

inline std::expected<std::unique_ptr<CallGraphSnapshot>, std::string>
snapshotCallGraph(const cli::CallGraphOptions &options,
                  const CallGraphRequest &request,
                  const RecoveryResult &current) {
  auto snapshot = std::make_unique<CallGraphSnapshot>(
      current.graph, current.coverage, CallGraphResult{});
  return queryCallGraph(options, request, snapshot->graph, snapshot->evidence())
      .transform([&](auto result) {
        snapshot->result = std::move(result);
        return std::move(snapshot);
      });
}
} // namespace facts::commands
