#ifndef FACTS_TOOL_STORAGE_ENUMERATION_H
#define FACTS_TOOL_STORAGE_ENUMERATION_H

#include "model/Enumeration.h"
#include "storage/Storage.h"

#include <expected>
#include <string_view>
#include <system_error>
#include <vector>

namespace facts {

template <>
std::expected<SymbolId, std::error_code>
Storage::save<Enumeration>(const Enumeration &enumeration);

template <>
std::expected<Enumeration, std::error_code>
Storage::load<Enumeration>(SymbolId id);

template <>
std::expected<std::optional<Enumeration>, std::error_code>
Storage::load<Enumeration>(std::string_view usr);

} // namespace facts

#endif
