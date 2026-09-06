#include "analysis/callgraph/CallGraphEntryQuery.h"

#include <llvm/Support/JSON.h>
#include <llvm/Support/raw_ostream.h>

#include <utility>

namespace facts::callgraph {
namespace {
std::string id(SymbolId value) { return std::to_string(value.packed()); }

llvm::json::Object coverageJson(const QueryNode &node,
                                const CoverageReport *coverage) {
  const auto *file =
      coverage ? findCoverageEvidenceFile(*coverage, node) : nullptr;
  llvm::json::Object result{
      {"state", coverage ? extractionCoverage(*coverage, node) : "unknown"},
      {"freshness", coverage ? coverageFreshness(*coverage, node) : "unknown"},
      {"action",
       coverage ? coverageAction(*coverage, node) : "supply-project-conf"},
      {"catalog_indexed",
       file ? llvm::json::Value(file->indexed) : llvm::json::Value(nullptr)},
      {"indexed_at", file && !file->indexedAt.empty()
                         ? llvm::json::Value(file->indexedAt)
                         : llvm::json::Value(nullptr)},
      {"catalog_mtime", file && file->mtime ? llvm::json::Value(*file->mtime)
                                            : llvm::json::Value(nullptr)},
      {"failure", nullptr}};
  return result;
}
} // namespace

std::string renderCallGraphEntryJson(const QueryNode &node,
                                     const EntryRecord &record,
                                     const CoverageReport *coverage) {
  const auto available = record.entry.has_value();
  const auto leaf = available && record.leaf;
  llvm::json::Array targets;
  for (const auto &target : record.externalTargets) {
    targets.push_back(llvm::json::Object{
        {"symbol_id", id(target.id)},
        {"external_symbol_id", id(target.id)},
        {"name", target.name},
        {"usr", target.usr},
        {"site",
         llvm::json::Object{{"source_id", id(node.id)},
                            {"destination_id", id(target.id)},
                            {"kind", static_cast<unsigned>(target.kind)},
                            {"position", target.position},
                            {"file_id", target.file},
                            {"offset", target.offset},
                            {"line", target.line},
                            {"column", target.column}}}});
  }
  auto evidence = coverageJson(node, coverage);
  evidence["unresolved_targets"] = node.unresolved;
  const auto availability = coverage ? definitionAvailability(*coverage, node)
                            : node.definition ? "available"
                            : node.external   ? "external-unavailable"
                                              : "unknown";
  const auto extraction = record.aggregateCoverage;
  llvm::json::Array candidates;
  if (coverage)
    for (const auto &path : coverage->recoveryCandidates)
      candidates.push_back(path);
  llvm::json::Object output{
      {"schema_version", 1},
      {"symbol_id", id(node.id)},
      {"usr", node.usr},
      {"entry_available", available},
      {"graph_node_ref", available
                             ? llvm::json::Value(id(record.entry->graphNodeRef))
                             : llvm::json::Value(nullptr)},
      {"is_leaf",
       available ? llvm::json::Value(leaf) : llvm::json::Value(nullptr)},
      {"external_targets", std::move(targets)},
      {"pair",
       llvm::json::Object{{"state", coverage ? "validated" : "unavailable"}}},
      {"definition_availability", availability},
      {"extraction_coverage",
       llvm::json::Object{{"state", extraction},
                          {"failure", nullptr},
                          {"recovery_candidates", std::move(candidates)}}},
      {"coverage", std::move(evidence)}};
  std::string text;
  llvm::raw_string_ostream stream(text);
  stream << llvm::json::Value(std::move(output)) << '\n';
  return text;
}

} // namespace facts::callgraph
