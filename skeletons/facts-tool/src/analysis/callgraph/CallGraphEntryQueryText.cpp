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
  const auto leaf = available && record.leaf && record.pointerCalls.empty();
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
      "external_targets={} pointer_calls={} pair={} definition_availability={} "
      "extraction_coverage={} freshness={} unresolved_targets={}\n",
      record.externalTargets.size(), record.pointerCalls.size(),
      coverage ? "validated" : "unavailable",
      availability, extraction, freshness, node.unresolved);
  for (const auto &call : record.pointerCalls)
    output += std::format(
        "pointer-call target={} usr={} signature={} expression={} "
        "site={}:{}:{} offset={}\n",
        call.site.target ? id(*call.site.target) : "null",
        call.target ? call.target->usr : "", call.site.signature,
        call.site.expression, call.site.file, call.site.location.line,
        call.site.location.column, call.site.location.offset);
  return output;
}

} // namespace facts::callgraph
