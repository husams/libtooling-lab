#include "storage/Enumeration.h"

namespace facts {

template <>
std::expected<SymbolId, std::error_code>
Storage::save<Enumeration>(const Enumeration &enumeration) {
  return saveModel(SymbolNode::Enumeration, enumeration, {});
}

template <>
std::expected<Enumeration, std::error_code>
Storage::load<Enumeration>(SymbolId id) {
  return loadModel<Enumeration>(SymbolNode::Enumeration, id, {});
}

template <>
std::expected<std::optional<Enumeration>, std::error_code>
Storage::load<Enumeration>(std::string_view usr) {
  return loadModel<Enumeration>(SymbolNode::Enumeration, usr, {});
}

} // namespace facts
