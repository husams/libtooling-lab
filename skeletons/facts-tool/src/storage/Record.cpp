#include "storage/Record.h"

namespace facts {

template <>
std::expected<SymbolId, std::error_code>
Storage::save<Record>(const Record &record) {
  return saveModel(SymbolNode::Record, record, {.definition = true});
}

template <>
std::expected<Record, std::error_code> Storage::load<Record>(SymbolId id) {
  return loadModel<Record>(SymbolNode::Record, id, {.definition = true});
}

template <>
std::expected<std::optional<Record>, std::error_code>
Storage::load<Record>(std::string_view usr) {
  return loadModel<Record>(SymbolNode::Record, usr, {.definition = true});
}

} // namespace facts
