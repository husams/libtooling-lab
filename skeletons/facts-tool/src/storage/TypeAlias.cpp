#include "storage/TypeAlias.h"

namespace facts {

template <>
std::expected<SymbolId, std::error_code>
Storage::save<TypeAlias>(const TypeAlias &typeAlias) {
  return saveModel(SymbolNode::TypeAlias, typeAlias, {});
}

template <>
std::expected<TypeAlias, std::error_code>
Storage::load<TypeAlias>(SymbolId id) {
  return loadModel<TypeAlias>(SymbolNode::TypeAlias, id, {});
}

template <>
std::expected<std::optional<TypeAlias>, std::error_code>
Storage::load<TypeAlias>(std::string_view usr) {
  return loadModel<TypeAlias>(SymbolNode::TypeAlias, usr, {});
}

} // namespace facts
