#include "commands/analyse/RecoveryEvidence.h"
#include "commands/analyse/RecoveryEvidenceDefinition.h"

#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "commands/analyse/RecoveryEvidenceScanner.h"
#include "storage/catalog/File.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <tuple>
#include <vector>

namespace facts::commands {
using CallKey =
    std::tuple<std::string, std::string, unsigned, unsigned, unsigned, bool>;

static std::expected<void, std::string> validateCalls(
    const RecoveryContext &context, const callgraph::QueryGraph &graph,
    const callgraph::QueryNode &node, const RecoveryBodyFacts &facts) {
  std::set<CallKey> expected, persisted;
  const auto calls = facts.calls.find(node.usr);
  if (calls == facts.calls.end())
    return std::unexpected("body call evidence is absent");
  for (const auto &call : calls->second)
    expected.insert(
        {call.destination,
         std::filesystem::path(call.path).lexically_normal().string(),
         call.offset, call.line, call.column, call.implicit});
  const auto unresolved = facts.unresolved.find(node.usr);
  if ((unresolved == facts.unresolved.end() ? 0U : unresolved->second) !=
      node.unresolved)
    return std::unexpected("unresolved call evidence changed");
  bool hasDispatch = false;
  for (const auto &edge : graph.edges) {
    if (edge.source != node.id)
      continue;
    if (edge.kind == RelationKind::DispatchCalls) {
      hasDispatch = true;
      continue;
    }
    if (edge.kind != RelationKind::Calls)
      continue;
    const auto target = std::ranges::find(graph.nodes, edge.destination,
                                          &callgraph::QueryNode::id);
    const auto file = context.files.find(edge.file);
    if (target == graph.nodes.end() || file == context.files.end())
      return std::unexpected("call evidence file is not registered");
    const auto path = catalog::filePath(file->second);
    if (!path)
      return std::unexpected(path.error());
    persisted.insert({target->usr, path->lexically_normal().string(),
                      edge.offset, edge.line, edge.column, edge.implicit});
  }
  if (hasDispatch && !node.bodyEvidence)
    return std::unexpected("dispatch evidence lacks persisted body proof");
  return expected == persisted
             ? std::expected<void, std::string>{}
             : std::unexpected("persisted call evidence is incomplete");
}

std::expected<RecoveryEvidence, std::string> validateRecoveryEvidence(
    const RecoveryContext &context, const cli::CallGraphOptions &,
    const callgraph::QueryGraph &graph, const RecoveryCandidate &candidate) {
  auto facts = scanRecoveryBody(context, candidate);
  if (!facts || facts->unsupported)
    return std::unexpected(facts ? "unsupported call evidence" : facts.error());
  std::vector<SymbolId> valid;
  for (const auto &usr : candidate.entry.relatedUsrs) {
    const auto node =
        std::ranges::find(graph.nodes, usr, &callgraph::QueryNode::usr);
    if (node == graph.nodes.end())
      return std::unexpected("requested evidence symbol is absent");
    if (auto checked = validateRecoveryDefinition(context, *node, *facts);
        !checked)
      return std::unexpected(checked.error());
    if (auto checked = validateCalls(context, graph, *node, *facts); !checked)
      return std::unexpected(checked.error());
    valid.push_back(node->id);
  }
  return makeRecoveryEvidence(valid);
}
} // namespace facts::commands
