#include "storage/Variable.h"

namespace facts {

template <>
std::expected<SymbolId, std::error_code>
Storage::save<Variable>(const Variable &variable) {
  return saveModel(SymbolNode::Variable, variable, {});
}

template <>
std::expected<Variable, std::error_code> Storage::load<Variable>(SymbolId id) {
  return loadModel<Variable>(SymbolNode::Variable, id, {});
}

template <>
std::expected<std::optional<Variable>, std::error_code>
Storage::load<Variable>(std::string_view usr) {
  return loadModel<Variable>(SymbolNode::Variable, usr, {});
}

} // namespace facts
