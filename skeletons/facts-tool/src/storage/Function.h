#ifndef FACTS_TOOL_STORAGE_FUNCTION_H
#define FACTS_TOOL_STORAGE_FUNCTION_H

#include "model/Function.h"
#include "storage/Storage.h"

#include <expected>
#include <string_view>
#include <system_error>
#include <vector>

namespace facts {

template <>
std::expected<SymbolId, std::error_code>
Storage::save<Function>(const Function &function);

template <>
std::expected<Function, std::error_code> Storage::load<Function>(SymbolId id);

template <>
std::expected<std::optional<Function>, std::error_code>
Storage::load<Function>(std::string_view usr);

} // namespace facts

#endif
