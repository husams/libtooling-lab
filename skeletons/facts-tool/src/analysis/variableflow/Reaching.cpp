#include "analysis/variableflow/Reaching.h"

#include <algorithm>
#include <map>
#include <ranges>
#include <set>
#include <tuple>
#include <unordered_map>

namespace facts::variableflow::detail {
namespace {

using DefinitionSet = std::set<std::int64_t>;
using GroupKey = std::pair<std::string, std::string>;

struct OrderedEvent {
  const AccessEvent *event = nullptr;
  int block = -1;
};

bool reads(Access kind) {
  return kind == Access::Read || kind == Access::Update;
}

bool writes(Access kind) {
  return kind == Access::Write || kind == Access::Update ||
         kind == Access::MayWrite;
}

bool kills(Access kind) {
  return kind == Access::Write || kind == Access::Update;
}

unsigned nodeDepth(const Builder &builder, std::int64_t id) {
  const auto found = std::ranges::find_if(
      builder.graph.nodes, [&](const auto &node) { return node.id == id; });
  return found == builder.graph.nodes.end() ? 0 : found->depth;
}

void unmapped(Builder &builder, const AccessEvent &event, std::string detail) {
  builder.graph.status = "partial";
  builder.boundary(event.node, "cfg-unmapped", std::move(detail),
                   nodeDepth(builder, event.node));
}

int nodeBlock(const Builder &builder, const AccessEvent &event,
              const Blocks &blocks) {
  if (event.block >= 0)
    return event.block;
  if (event.kind != Access::Write)
    return -1;
  const auto found =
      std::ranges::find_if(builder.graph.nodes, [&](const auto &node) {
        return node.id == event.node;
      });
  return found != builder.graph.nodes.end() && found->kind == "parameter"
             ? blocks.entry
             : -1;
}

DefinitionSet transfer(const DefinitionSet &incoming,
                       const std::vector<OrderedEvent> &events) {
  auto current = incoming;
  for (const auto &entry : events) {
    if (!writes(entry.event->kind))
      continue;
    if (kills(entry.event->kind))
      current.clear();
    current.insert(entry.event->node);
  }
  return current;
}

void addDataEdges(
    Builder &builder, std::int64_t target, const DefinitionSet &definitions,
    std::set<std::tuple<std::int64_t, std::int64_t, std::string>> &seen) {
  for (const auto definition : definitions) {
    const auto key = std::make_tuple(definition, target, std::string{"data"});
    if (seen.insert(key).second)
      builder.edge(definition, target, "data");
  }
}

void emitBlock(
    Builder &builder, const std::vector<OrderedEvent> &events,
    DefinitionSet current,
    std::set<std::tuple<std::int64_t, std::int64_t, std::string>> &seen) {
  for (const auto &entry : events) {
    if (reads(entry.event->kind))
      addDataEdges(builder, entry.event->node, current, seen);
    if (!writes(entry.event->kind))
      continue;
    if (kills(entry.event->kind))
      current.clear();
    current.insert(entry.event->node);
  }
}

void oneGroup(Builder &builder, const GroupKey &key,
              const std::vector<const AccessEvent *> &events) {
  const auto graph = builder.blockGraphs.find(key.first);
  if (graph == builder.blockGraphs.end()) {
    for (const auto *event : events)
      unmapped(builder, *event, "CFG unavailable for function " + key.first);
    return;
  }

  const auto &blocks = graph->second;
  std::vector<OrderedEvent> ordered;
  ordered.reserve(events.size());
  for (const auto *event : events) {
    const auto block = nodeBlock(builder, *event, blocks);
    if (block < 0 || !blocks.reachable.contains(block)) {
      unmapped(builder, *event,
               event->block < 0 ? "event has no CFG block"
                                : "event is in an unreachable CFG block");
      ordered.push_back(OrderedEvent{event, -1});
      continue;
    }
    ordered.push_back(OrderedEvent{event, block});
  }

  std::map<int, std::vector<OrderedEvent>> byBlock;
  for (const auto entry : ordered) {
    if (entry.block >= 0)
      byBlock[entry.block].push_back(entry);
  }
  const auto eventOrder = [](const OrderedEvent &left,
                             const OrderedEvent &right) {
    if (left.event->statementOrder != right.event->statementOrder)
      return left.event->statementOrder < right.event->statementOrder;
    return left.event->sequence < right.event->sequence;
  };
  for (auto &[ignored, blockEvents] : byBlock)
    std::ranges::sort(blockEvents, eventOrder);

  std::unordered_map<int, DefinitionSet> in;
  std::unordered_map<int, DefinitionSet> out;
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto &[block, ignored] : blocks.nodes) {
      DefinitionSet nextIn;
      if (const auto predecessors = blocks.predecessors.find(block);
          predecessors != blocks.predecessors.end())
        for (const auto predecessor : predecessors->second)
          if (const auto found = out.find(predecessor); found != out.end())
            nextIn.insert(found->second.begin(), found->second.end());
      const auto found = byBlock.find(block);
      const auto nextOut =
          found == byBlock.end() ? nextIn : transfer(nextIn, found->second);
      changed |= nextIn != in[block] || nextOut != out[block];
      in[block] = std::move(nextIn);
      out[block] = std::move(nextOut);
    }
  }

  std::set<std::tuple<std::int64_t, std::int64_t, std::string>> seen;
  for (const auto &[block, blockEvents] : byBlock)
    emitBlock(builder, blockEvents, in[block], seen);
}

} // namespace

void applyReachingDefinitions(Builder &builder) {
  std::map<GroupKey, std::vector<const AccessEvent *>> groups;
  for (const auto &event : builder.events)
    groups[{event.functionUsr, event.variableUsr}].push_back(&event);
  for (const auto &[key, events] : groups)
    oneGroup(builder, key, events);
}

} // namespace facts::variableflow::detail
