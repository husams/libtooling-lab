#include "analysis/callgraph/CallGraphJsonRecovery.h"

#include "analysis/callgraph/CallGraphJsonDetail.h"

namespace facts::callgraph::json {
namespace {

llvm::json::Array entries(const std::vector<RecoveryEntry> &entries) {
  llvm::json::Array result;
  for (const auto &entry : entries) {
    llvm::json::Array arguments;
    for (const auto &argument : entry.arguments)
      arguments.push_back(argument);
    llvm::json::Array usrs;
    for (const auto &usr : entry.relatedUsrs)
      usrs.push_back(usr);
    llvm::json::Object value{
        {"tu_file_id", entry.tuFileId == 0
                            ? llvm::json::Value(nullptr)
                            : llvm::json::Value(std::to_string(entry.tuFileId))},
        {"component", entry.component},
        {"driver", entry.driver},
        {"working_directory", entry.workingDirectory},
        {"arguments", std::move(arguments)},
        {"related_usrs", std::move(usrs)},
        {"reason", entry.reason}};
    result.push_back(std::move(value));
  }
  return result;
}

llvm::json::Array missingDefinitions(const QueryGraph &graph,
                                     std::span<const SymbolId> nodes) {
  llvm::json::Array result;
  for (const auto id : nodes)
    if (const auto *node = detail::findNode(graph, id))
      if (!node->definition && !node->implicit)
        result.push_back(detail::stableId(node->id));
  return result;
}

llvm::json::Array unresolvedTargets(const QueryGraph &graph,
                                    std::span<const SymbolId> nodes) {
  llvm::json::Array result;
  for (const auto id : nodes)
    if (const auto *node = detail::findNode(graph, id))
      if (node->unresolved)
        result.push_back(detail::stableId(node->id));
  return result;
}

} // namespace

RecoverySections recoverySections(const QueryGraph &graph,
                                  const CoverageReport *coverage,
                                  std::span<const SymbolId> nodes) {
  RecoverySections result;
  if (coverage)
    for (const auto &path : coverage->recoveryCandidates)
      result.candidates.push_back(path);
  result.missingDefinitions = missingDefinitions(graph, nodes);
  result.unresolvedTargets = unresolvedTargets(graph, nodes);
  result.extractionCoverage["state"] =
      coverage ? summarizeCoverage(*coverage, graph, nodes)
               : std::string{"unknown"};
  result.extractionCoverage["failure"] = nullptr;
  result.extractionCoverage["recovery_candidates"] =
      llvm::json::Value(llvm::json::Array(result.candidates));
  result.extractionCoverage["unsupported_semantics"] =
      llvm::json::Object{{"state", "not-persisted"},
                         {"action", "inspect-extraction-diagnostics"},
                         {"sites", llvm::json::Array{}}};
  return result;
}

llvm::json::Object recoveryObject(const RecoveryReport &report) {
  return llvm::json::Object{
      {"requested", report.requested},
      {"attempted", entries(report.attempted)},
      {"reused", entries(report.reused)},
      {"failed", entries(report.failed)},
      {"suppressed", entries(report.suppressed)}};
}

llvm::json::Array recoveryErrors(const RecoveryReport *report) {
  if (!report || report->failed.empty())
    return {};
  return llvm::json::Array{llvm::json::Object{
      {"code", "recovery-failed"}, {"diagnostic", "recovery failed"}}};
}

} // namespace facts::callgraph::json
