#ifndef FACTS_TOOL_STORAGE_ENUMERATOR_H
#define FACTS_TOOL_STORAGE_ENUMERATOR_H

#include "model/Enumerator.h"
#include "storage/Storage.h"

#include <expected>
#include <string_view>
#include <system_error>

namespace facts {

template <>
std::expected<SymbolId, std::error_code>
Storage::save<Enumerator>(const Enumerator &enumerator);

template <>
std::expected<Enumerator, std::error_code>
Storage::load<Enumerator>(SymbolId id);

template <>
std::expected<std::optional<Enumerator>, std::error_code>
Storage::load<Enumerator>(std::string_view usr);

} // namespace facts

#endif
