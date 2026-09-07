#include "commands/analyse/CallGraphCompletion.h"

#include <algorithm>
#include <format>
#include <iostream>

namespace facts::commands {

std::string_view runStatusName(RunStatus status) {
  switch (status) {
  case RunStatus::Complete:
    return "complete";
  case RunStatus::Truncated:
    return "truncated";
  case RunStatus::Cancelled:
    return "cancelled";
  case RunStatus::RecoveryFailed:
    return "recovery-failed";
  case RunStatus::Failed:
    return "failed";
  }
  return "failed";
}

std::expected<int, std::string>
completeCallGraphRun(const CallGraphRunRecord &record, std::int64_t runId) {
  std::cout << "facts-tool: call graph run " << runId << ' '
            << runStatusName(record.status) << '\n';
  switch (record.status) {
  case RunStatus::Complete:
  case RunStatus::Truncated:
    return 0;
  case RunStatus::Failed:
    return std::unexpected("facts-tool: " + record.error);
  case RunStatus::RecoveryFailed:
    return std::unexpected(std::format(
        "facts-tool: recovery failed for {} translation unit(s); see "
        "callgraph_run_recovery run {}",
        std::ranges::count(record.recovery, "failed", &RunRecoveryRow::outcome),
        runId));
  case RunStatus::Cancelled:
    return std::unexpected(std::format(
        "facts-tool: cancelled; run {} keeps the last usable generation",
        runId));
  }
  return 1;
}
} // namespace facts::commands
