#include "analysis/callgraph/CallGraphJsonDetail.h"

namespace facts::callgraph::detail {
llvm::json::Value definitionJson(const QueryNode &node,
                                 const CoverageReport *coverage) {
  if (!node.definitionLocation)
    return nullptr;
  const auto *file =
      coverage ? findCoverageFile(*coverage, node.definitionLocation->file)
               : nullptr;
  llvm::json::Object location{{"file_id", node.definitionLocation->file},
                              {"offset", node.definitionLocation->offset},
                              {"size", node.definitionLocation->size}};
  location["path"] =
      file ? llvm::json::Value(file->path) : llvm::json::Value(nullptr);
  return location;
}
} // namespace facts::callgraph::detail
