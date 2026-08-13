#include "storage/Function.h"

namespace facts {

template <>
std::expected<SymbolId, std::error_code>
Storage::save<Function>(const Function &function) {
  return saveModel(SymbolNode::Function, function,
                   {.definition = true, .parameters = true});
}

template <>
std::expected<Function, std::error_code> Storage::load<Function>(SymbolId id) {
  return loadModel<Function>(SymbolNode::Function, id,
                             {.definition = true, .parameters = true});
}

template <>
std::expected<std::optional<Function>, std::error_code>
Storage::load<Function>(std::string_view usr) {
  return loadModel<Function>(SymbolNode::Function, usr,
                             {.definition = true, .parameters = true});
}

} // namespace facts
