#include "analysis/callgraph/CallGraphEntryQuery.h"

#include <format>

namespace facts::callgraph {
namespace {
std::string id(SymbolId value) { return std::to_string(value.packed()); }
} // namespace

std::string renderCallGraphEntryText(const QueryNode &node,
                                     const EntryRecord &record) {
  const auto available = record.entry.has_value();
  const auto leaf = available && record.leaf;
  std::string output = std::format(
      "symbol_id={} usr={} entry_available={} graph_node_ref={} is_leaf={}\n",
      id(node.id), node.usr, available,
      available ? id(record.entry->graphNodeRef) : "null",
      available ? (leaf ? "true" : "false") : "null");
  output += std::format(
      "external_targets={} freshness=unknown unresolved_targets={}\n",
      record.externalTargets.size(), node.unresolved);
  return output;
}

} // namespace facts::callgraph
