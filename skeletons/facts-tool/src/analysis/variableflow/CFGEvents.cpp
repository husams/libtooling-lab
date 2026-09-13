#include "analysis/variableflow/FlowSupport.h"

#include <clang/Analysis/CFG.h>

#include <deque>
#include <format>

namespace facts::variableflow::detail {

Blocks buildBlocks(const Function &function, Builder &builder, unsigned depth) {
  if (const auto found = builder.blockGraphs.find(function.usr);
      found != builder.blockGraphs.end())
    return found->second;
  Blocks result;
  clang::CFG::BuildOptions options;
  options.setAllAlwaysAdd();
  const auto cfg = clang::CFG::buildCFG(function.decl, function.decl->getBody(),
                                        function.context, options);
  if (!cfg)
    return result;

  const auto *entry = &cfg->getEntry();
  if (entry == nullptr)
    return result;
  result.entry = entry->getBlockID();
  std::deque<const clang::CFGBlock *> pending{entry};
  while (!pending.empty()) {
    const auto *block = pending.front();
    pending.pop_front();
    if (block == nullptr ||
        !result.reachable.insert(block->getBlockID()).second)
      continue;
    for (const auto successor : block->succs())
      if (const auto *next = successor.getReachableBlock())
        pending.push_back(next);
  }

  for (const auto *block : *cfg) {
    if (block == nullptr || !result.reachable.contains(block->getBlockID()))
      continue;
    const auto terminator = block->getTerminator().getStmt();
    const auto id = builder.node("control", function, nullptr,
                                 terminator ? terminator->getBeginLoc()
                                            : clang::SourceLocation{},
                                 block->getBlockID(), depth,
                                 std::format("block {}", block->getBlockID()));
    result.nodes.emplace(block->getBlockID(), id);
    unsigned order = 0;
    for (const auto &element : *block) {
      if (const auto statement = element.getAs<clang::CFGStmt>()) {
        result.statements.emplace(statement->getStmt(), block->getBlockID());
        result.statementOrder.try_emplace(statement->getStmt(), order);
      }
      ++order;
    }
  }

  for (const auto *block : *cfg) {
    if (block == nullptr || !result.nodes.contains(block->getBlockID()))
      continue;
    for (const auto successor : block->succs()) {
      const auto *next = successor.getReachableBlock();
      if (next == nullptr || !result.nodes.contains(next->getBlockID()))
        continue;
      builder.edge(result.nodes.at(block->getBlockID()),
                   result.nodes.at(next->getBlockID()), "control");
      result.predecessors[next->getBlockID()].push_back(block->getBlockID());
    }
  }
  builder.blockGraphs[function.usr] = result;
  return result;
}

} // namespace facts::variableflow::detail
