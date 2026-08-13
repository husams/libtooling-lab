#ifndef FACTS_TOOL_STORAGE_VARIABLE_H
#define FACTS_TOOL_STORAGE_VARIABLE_H

#include "model/Variable.h"
#include "storage/Storage.h"

#include <expected>
#include <string_view>
#include <system_error>
#include <vector>

namespace facts {

template <>
std::expected<SymbolId, std::error_code>
Storage::save<Variable>(const Variable &variable);

template <>
std::expected<Variable, std::error_code> Storage::load<Variable>(SymbolId id);

template <>
std::expected<std::optional<Variable>, std::error_code>
Storage::load<Variable>(std::string_view usr);

} // namespace facts

#endif
