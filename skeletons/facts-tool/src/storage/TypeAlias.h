#ifndef FACTS_TOOL_STORAGE_TYPE_ALIAS_H
#define FACTS_TOOL_STORAGE_TYPE_ALIAS_H

#include "model/TypeAlias.h"
#include "storage/Storage.h"

#include <expected>
#include <string_view>
#include <system_error>
#include <vector>

namespace facts {

template <>
std::expected<SymbolId, std::error_code>
Storage::save<TypeAlias>(const TypeAlias &typeAlias);

template <>
std::expected<TypeAlias, std::error_code> Storage::load<TypeAlias>(SymbolId id);

template <>
std::expected<std::optional<TypeAlias>, std::error_code>
Storage::load<TypeAlias>(std::string_view usr);

} // namespace facts

#endif
