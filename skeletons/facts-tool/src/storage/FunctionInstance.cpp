#include "storage/FunctionInstance.h"

namespace facts {

template <>
std::expected<SymbolId, std::error_code>
Storage::save<FunctionInstance>(const FunctionInstance &function) {
  return saveModel(SymbolNode::Function, function,
                   {.definition = true, .parameters = true})
      .and_then([this, &function](SymbolId id) {
        return addTemplateArguments(id, function.templateArguments)
            .and_then([this, &function, id] {
              return addTemplateParameters(id, function.templateParameters);
            })
            .transform([id] { return id; });
      });
}

} // namespace facts
