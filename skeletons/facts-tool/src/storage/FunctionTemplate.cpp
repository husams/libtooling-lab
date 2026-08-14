#include "storage/FunctionTemplate.h"

namespace facts {

template <>
std::expected<SymbolId, std::error_code>
Storage::save<FunctionTemplate>(const FunctionTemplate &function) {
  return saveModel(SymbolNode::Function, function,
                   {.definition = true, .parameters = true})
      .and_then([this, &function](SymbolId id) {
        return addTemplateArguments(id, function.templateArguments)
            .transform([id] { return id; });
      });
}

} // namespace facts
