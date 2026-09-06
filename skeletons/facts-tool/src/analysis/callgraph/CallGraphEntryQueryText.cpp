#include "analysis/callgraph/CallGraphEntryQuery.h"

#include <format>

namespace facts::callgraph {
namespace {
std::string id(SymbolId value) { return std::to_string(value.packed()); }
} // namespace

std::string renderCallGraphEntryText(const QueryNode &node,
                                     const EntryRecord &record,
                                     const CoverageReport *coverage) {
  const auto available = record.entry.has_value();
  const auto leaf = available && record.leaf;
  const auto availability = coverage ? definitionAvailability(*coverage, node)
                            : node.definition ? "available"
                            : node.external   ? "external-unavailable"
                                              : "unknown";
  const auto extraction = record.aggregateCoverage;
  const auto freshness =
      coverage ? coverageFreshness(*coverage, node) : "unknown";
  std::string output = std::format(
      "symbol_id={} usr={} entry_available={} graph_node_ref={} is_leaf={}\n",
      id(node.id), node.usr, available,
      available ? id(record.entry->graphNodeRef) : "null",
      available ? (leaf ? "true" : "false") : "null");
  output += std::format(
      "external_targets={} pair={} definition_availability={} "
      "extraction_coverage={} freshness={} unresolved_targets={}\n",
      record.externalTargets.size(), coverage ? "validated" : "unavailable",
      availability, extraction, freshness, node.unresolved);
  return output;
}

} // namespace facts::callgraph
