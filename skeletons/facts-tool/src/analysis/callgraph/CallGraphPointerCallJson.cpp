#include "analysis/callgraph/CallGraphPointerCallJson.h"

namespace facts::callgraph {
namespace {
std::string id(SymbolId value) { return std::to_string(value.packed()); }
} // namespace

llvm::json::Object pointerCallJson(const QueryPointerCall &call) {
  llvm::json::Value target = nullptr;
  if (call.target)
    target = llvm::json::Object{{"symbol_id", id(call.target->id)},
                                {"name", call.target->name},
                                {"usr", call.target->usr}};
  return llvm::json::Object{
      {"kind", "pointer-call"},
      {"target", std::move(target)},
      {"signature", call.site.signature},
      {"expression", call.site.expression},
      {"site",
       llvm::json::Object{
           {"source_id", id(call.site.source)},
           {"target_id", call.site.target
                             ? llvm::json::Value(id(*call.site.target))
                             : llvm::json::Value(nullptr)},
           {"file_id", call.site.file},
           {"offset", call.site.location.offset},
           {"line", call.site.location.line},
           {"column", call.site.location.column}}}};
}

} // namespace facts::callgraph
