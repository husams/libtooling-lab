#include "analysis/variableflow/Reaching.h"
#include "analysis/variableflow/Traversal.h"

#include <algorithm>
#include <ranges>

namespace facts::variableflow::detail {
namespace {

bool localDependency(const Edge &edge) {
  return edge.kind == "value" || edge.kind == "argument" ||
         edge.kind == "call-result" || edge.kind == "alias";
}

} // namespace

Summary &Traversal::summary(const Function &function, unsigned depth) {
  if (const auto found = summaries.find(function.usr);
      found != summaries.end()) {
    auto &result = *found->second;
    if (depth < result.depth) {
      result.depth = depth;
      for (auto &node : builder.graph.nodes)
        if (node.functionUsr == function.usr)
          node.depth = depth;
      for (const auto &call : result.calls)
        if (active.contains(call.node))
          pending.push_back(call.node);
    }
    return result;
  }
  const auto firstEdge = builder.graph.edges.size();
  auto value =
      std::make_unique<Summary>(collectSummary(function, builder, depth));
  auto &result = *value;
  summaries.emplace(function.usr, std::move(value));
  for (const auto id : result.nodes)
    owners.emplace(id, &result);
  for (const auto &call : result.calls)
    calls.emplace(call.node, &call);
  for (auto index = firstEdge; index < builder.graph.edges.size(); ++index) {
    const auto &edge = builder.graph.edges[index];
    if (localDependency(edge)) {
      localLinks[edge.source].push_back(edge.target);
      localLinks[edge.target].push_back(edge.source);
    }
  }
  return result;
}

void Traversal::activate(std::int64_t id) {
  if (id != 0 && active.insert(id).second)
    pending.push_back(id);
}

void Traversal::drain() {
  while (!pending.empty()) {
    if (request.cancelled && request.cancelled()) return;
    const auto id = pending.front();
    pending.pop_front();
    const auto found = owners.find(id);
    if (found == owners.end())
      continue;
    auto &owner = *found->second;
    if (const auto variable = owner.variableOf.find(id);
        variable != owner.variableOf.end())
      for (const auto occurrence : owner.variables[variable->second])
        activate(occurrence);
    for (const auto neighbor : localLinks[id])
      activate(neighbor);
    if (const auto call = calls.find(id); call != calls.end())
      if (!expanded.contains(id) || owner.depth < expanded.at(id)) {
        expanded[id] = owner.depth;
        expandCall(owner, *call->second);
      }
  }
}

Graph Traversal::finish() {
  std::unordered_set<std::string> used;
  for (const auto &node : builder.graph.nodes)
    if (active.contains(node.id))
      used.insert(node.functionUsr);
  for (const auto &[usr, owner] : summaries) {
    if (!used.contains(usr))
      continue;
    active.insert(owner->owner);
    for (const auto &[block, id] : owner->blocks.nodes) {
      active.insert(id);
      builder.edge(owner->owner, id, "contains");
    }
  }
  for (const auto &node : builder.graph.nodes) {
    if (!active.contains(node.id) || !summaries.contains(node.functionUsr))
      continue;
    const auto &owner = *summaries.at(node.functionUsr);
    if (node.id == owner.owner || node.kind == "control")
      continue;
    const auto block = owner.blocks.nodes.find(node.block);
    builder.edge(block == owner.blocks.nodes.end() ? owner.owner
                                                   : block->second,
                 node.id, "contains");
  }
  std::erase_if(builder.graph.nodes,
                [&](const auto &node) { return !active.contains(node.id); });
  std::erase_if(builder.events, [&](const auto &event) {
    return !active.contains(event.node);
  });
  std::erase_if(builder.graph.edges, [&](const auto &edge) {
    return !active.contains(edge.source) || !active.contains(edge.target);
  });
  std::erase_if(builder.graph.boundaries, [&](const auto &boundary) {
    return !active.contains(boundary.node) ||
           (boundary.reason == "depth-limit" && request.maxDepth &&
            owners.at(boundary.node)->depth < *request.maxDepth);
  });
  builder.graph.status = "complete";
  for (const auto &boundary : builder.graph.boundaries)
    if (boundary.reason != "external" && boundary.reason != "depth-limit")
      builder.graph.status = "partial";
  applyReachingDefinitions(builder);
  return std::move(builder.graph);
}

Graph runFlow(const Parsed &parsed, const Function &root,
              const clang::VarDecl *variable, const Request &request) {
  Traversal traversal{parsed, request};
  traversal.builder.graph.rootFunction = root.usr;
  traversal.builder.graph.rootVariable = usrFor(*variable, root.tu);
  auto &summary = traversal.summary(root, 0);
  for (const auto id : summary.variables[variable])
    traversal.activate(id);
  // Calls activate parameter and result dependencies; newly discovered effects
  // feed caller variables back into this finite summary worklist.
  bool changed = false;
  do {
    traversal.drain();
    if (request.cancelled && request.cancelled()) {
      Graph cancelled;
      cancelled.status = "cancelled";
      return cancelled;
    }
    const auto priorEdges = traversal.builder.graph.edges.size();
    traversal.linkEffects();
    changed = priorEdges != traversal.builder.graph.edges.size();
  } while (changed || !traversal.pending.empty());
  return traversal.finish();
}

} // namespace facts::variableflow::detail
