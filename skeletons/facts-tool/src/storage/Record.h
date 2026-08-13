#ifndef FACTS_TOOL_STORAGE_RECORD_H
#define FACTS_TOOL_STORAGE_RECORD_H

#include "model/Record.h"
#include "storage/Storage.h"

#include <expected>
#include <string_view>
#include <system_error>
#include <vector>

namespace facts {

template <>
std::expected<SymbolId, std::error_code>
Storage::save<Record>(const Record &record);

template <>
std::expected<Record, std::error_code> Storage::load<Record>(SymbolId id);

template <>
std::expected<std::optional<Record>, std::error_code>
Storage::load<Record>(std::string_view usr);

} // namespace facts

#endif
