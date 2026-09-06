#include "analysis/callgraph/CallGraphEntryQuery.h"

#include <llvm/Support/JSON.h>
#include <llvm/Support/raw_ostream.h>

#include <utility>

namespace facts::callgraph {
namespace {
std::string id(SymbolId value) { return std::to_string(value.packed()); }
} // namespace

std::string renderCallGraphEntryJson(const QueryNode &node,
                                     const EntryRecord &record) {
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
  llvm::json::Object coverage{{"freshness", "unknown"},
                              {"state", "unknown"},
                              {"unresolved_targets", node.unresolved}};
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
      {"coverage", std::move(coverage)}};
  std::string text;
  llvm::raw_string_ostream stream(text);
  stream << llvm::json::Value(std::move(output)) << '\n';
  return text;
}

} // namespace facts::callgraph
