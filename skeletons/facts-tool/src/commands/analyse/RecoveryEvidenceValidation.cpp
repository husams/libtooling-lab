#include "commands/analyse/RecoveryEvidence.h"

#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "commands/analyse/RecoveryEvidenceScanner.h"
#include "storage/catalog/File.h"

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <tuple>
#include <vector>

namespace facts::commands {
namespace {
using CallKey =
    std::tuple<std::string, std::string, unsigned, unsigned, unsigned>;

std::expected<void, std::string>
validateDefinition(const RecoveryContext &context,
                   const callgraph::QueryNode &node,
                   const RecoveryBodyFacts &facts) {
  if (!node.definition || !node.definitionLocation || node.unresolved)
    return std::unexpected("incomplete persisted body evidence");
  const auto file = context.files.find(node.definitionLocation->file);
  if (file == context.files.end())
    return std::unexpected("definition file is not registered");
  const auto path = catalog::filePath(file->second);
  const auto proof = facts.definitions.find(node.usr);
  if (!path || proof == facts.definitions.end() ||
      std::filesystem::path(proof->second.path).lexically_normal() !=
          path->lexically_normal() ||
      node.definitionLocation->offset != proof->second.offset ||
      node.definitionLocation->size != proof->second.size)
    return std::unexpected("definition extent changed");
  return {};
}

std::expected<void, std::string> validateCalls(
    const RecoveryContext &context, const callgraph::QueryGraph &graph,
    const callgraph::QueryNode &node, const RecoveryBodyFacts &facts) {
  std::map<CallKey, unsigned> expected, persisted;
  for (const auto &call : facts.calls.at(node.usr))
    ++expected[{call.destination,
                std::filesystem::path(call.path).lexically_normal().string(),
                call.offset, call.line, call.column}];
  for (const auto &edge : graph.edges) {
    if (edge.source != node.id)
      continue;
    if (edge.implicit)
      return std::unexpected("implicit call evidence is unsupported");
    if (edge.kind == RelationKind::DispatchCalls)
      return std::unexpected("dispatch call evidence is unsupported");
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
    ++persisted[{target->usr, path->lexically_normal().string(), edge.offset,
                 edge.line, edge.column}];
  }
  return expected == persisted
             ? std::expected<void, std::string>{}
             : std::unexpected("persisted call evidence is incomplete");
}
} // namespace

std::expected<RecoveryEvidence, std::string> validateRecoveryEvidence(
    const RecoveryContext &context, const cli::CallGraphOptions &,
    const callgraph::QueryGraph &graph, const RecoveryCandidate &candidate) {
  auto facts = scanRecoveryBody(candidate);
  if (!facts || facts->unsupported)
    return std::unexpected(facts ? "unsupported call evidence" : facts.error());
  std::vector<SymbolId> valid;
  for (const auto &usr : candidate.entry.relatedUsrs) {
    const auto node =
        std::ranges::find(graph.nodes, usr, &callgraph::QueryNode::usr);
    if (node == graph.nodes.end())
      return std::unexpected("requested evidence symbol is absent");
    if (auto checked = validateDefinition(context, *node, *facts); !checked)
      return std::unexpected(checked.error());
    if (auto checked = validateCalls(context, graph, *node, *facts); !checked)
      return std::unexpected(checked.error());
    valid.push_back(node->id);
  }
  return makeRecoveryEvidence(valid);
}
} // namespace facts::commands
