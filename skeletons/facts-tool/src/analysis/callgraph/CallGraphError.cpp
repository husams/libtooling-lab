#include "analysis/callgraph/CallGraphError.h"

#include <llvm/Support/JSON.h>
#include <llvm/Support/raw_ostream.h>

namespace facts::callgraph {

std::string renderCallGraphErrorJson(std::string message) {
  llvm::json::Object output{
      {"schema", "facts-tool.call-graph.v1"},
      {"complete", false},
      {"coverage", llvm::json::Object{{"traversal_complete", false}}},
      {"truncation", llvm::json::Object{{"reached", true},
                                        {"reason", "error"},
                                        {"frontier", llvm::json::Array{}}}},
      {"excluded_scope", llvm::json::Object{}},
      {"recovery", llvm::json::Object{}},
      {"errors", llvm::json::Array{llvm::json::Object{{"kind", "operational"},
                                                      {"message", message}}}}};
  std::string text;
  llvm::raw_string_ostream stream(text);
  stream << llvm::json::Value(std::move(output)) << '\n';
  return text;
}

} // namespace facts::callgraph
