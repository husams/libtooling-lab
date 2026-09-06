#include "analysis/callgraph/CallGraphSelection.h"

#include <format>
#include <ranges>

namespace facts::callgraph {
namespace {

std::string identity(const QueryNode &node) {
  return std::format("name='{}' usr='{}' id='{}:{}'", node.name, node.usr,
                     node.id.file, node.id.index);
}

std::string candidates(const std::vector<const QueryNode *> &nodes) {
  std::string result;
  for (const auto *node : nodes)
    result += (result.empty() ? "" : ", ") + identity(*node);
  return result;
}

} // namespace

std::expected<const QueryNode *, std::string>
selectOne(const QueryGraph &graph, std::string_view selector,
          std::string_view role) {
  std::vector<const QueryNode *> matches;
  for (const auto &node : graph.nodes)
    if (node.name == selector || node.usr == selector)
      matches.push_back(&node);
  if (matches.empty())
    return std::unexpected(
        std::format("missing-{}: selector '{}' was not found", role, selector));
  if (matches.size() != 1)
    return std::unexpected(
        std::format("ambiguous-{}: selector '{}' matches [{}]; select a USR",
                    role, selector, candidates(matches)));
  return matches.front();
}

} // namespace facts::callgraph
