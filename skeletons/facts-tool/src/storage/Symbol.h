#ifndef FACTS_TOOL_STORAGE_SYMBOL_H
#define FACTS_TOOL_STORAGE_SYMBOL_H

#include "model/Symbol.h"
#include "storage/Storage.h"

#include <expected>
#include <string_view>
#include <system_error>
#include <vector>

namespace facts {

template <>
std::expected<SymbolId, std::error_code>
Storage::save<Symbol>(const Symbol &symbol);

template <>
std::expected<Symbol, std::error_code> Storage::load<Symbol>(SymbolId id);

template <>
std::expected<std::optional<Symbol>, std::error_code>
Storage::load<Symbol>(std::string_view usr);

} // namespace facts

#endif
