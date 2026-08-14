#ifndef FACTS_TOOL_STORAGE_RECORDTEMPLATE_H
#define FACTS_TOOL_STORAGE_RECORDTEMPLATE_H

#include "model/RecordTemplate.h"
#include "storage/Storage.h"

#include <expected>
#include <system_error>

namespace facts {

template <>
std::expected<SymbolId, std::error_code>
Storage::save<RecordTemplate>(const RecordTemplate &record);

} // namespace facts

#endif // FACTS_TOOL_STORAGE_RECORDTEMPLATE_H
