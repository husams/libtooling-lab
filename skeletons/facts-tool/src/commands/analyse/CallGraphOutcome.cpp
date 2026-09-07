#include "commands/analyse/CallGraphOutcome.h"

#include <algorithm>
#include <map>

namespace facts::commands {
namespace {
std::vector<RunRecoveryRow>
recoveryRows(const callgraph::RecoveryReport &report) {
  // failed > attempted > reused > suppressed when one TU appears twice.
  std::map<FileId, RunRecoveryRow> rows;
  const auto add = [&](const auto &entries, std::string_view outcome) {
    for (const auto &entry : entries) {
      if (entry.tuFileId == 0 || rows.contains(entry.tuFileId))
        continue;
      auto diagnostic = entry.reason;
      if (!entry.diagnostic.empty())
        diagnostic += (diagnostic.empty() ? "" : "\n") + entry.diagnostic;
      rows[entry.tuFileId] = {entry.tuFileId, std::string{outcome},
                              std::move(diagnostic)};
    }
  };
  add(report.failed, "failed");
  add(report.attempted, "attempted");
  add(report.reused, "reused");
  add(report.suppressed, "suppressed");
  std::vector<RunRecoveryRow> result;
  for (auto &[_, row] : rows)
    result.push_back(std::move(row));
  return result;
}

std::string operationalFailure(const callgraph::RecoveryReport *report) {
  if (!report)
    return {};
  const auto found = std::ranges::find(report->failed, FileId{0},
                                       &callgraph::RecoveryEntry::tuFileId);
  return found == report->failed.end() ? std::string{} : found->reason;
}
} // namespace

CallGraphRunRecord makeCallGraphRunRecord(
    const cli::CallGraphOptions &options, const CallGraphRequest &request,
    const CallGraphResult &result, const callgraph::RecoveryReport *recovery,
    bool cancelled, std::string error) {
  CallGraphRunRecord record;
  record.projectPath = options.configuration;
  record.factsPath = options.facts;
  record.mode = request.mode;
  if (request.mode == callgraph::QueryMode::Path)
    record.pathMode = request.pathMode;
  record.callsScope = result.traversal.scope.calls;
  record.components = options.components;
  record.limits = result.traversal.limits;
  record.recoverMissing = options.recoverMissing;
  for (const auto *root : result.roots)
    record.roots.emplace_back(root->id, root->usr);
  if (result.target)
    record.target = std::pair{result.target->id, result.target->usr};
  record.edges = result.traversal.edges;
  record.frontier = result.traversal.frontier;
  record.truncationReason = result.traversal.reason;
  if (recovery)
    record.recovery = recoveryRows(*recovery);
  if (error.empty())
    error = operationalFailure(recovery);
  const bool recoveryFailed = std::ranges::any_of(
      record.recovery, [](const auto &row) { return row.outcome == "failed"; });
  if (!error.empty()) {
    record.status = RunStatus::Failed;
    record.error = std::move(error);
  } else if (cancelled || result.traversal.reason == "cancelled") {
    record.status = RunStatus::Cancelled;
    record.truncationReason = "cancelled";
  } else if (recoveryFailed) {
    record.status = RunStatus::RecoveryFailed;
  } else if (result.traversal.truncated > 0) {
    record.status = RunStatus::Truncated;
  }
  return record;
}

} // namespace facts::commands
