#ifndef FACTS_TOOL_STORAGE_RECORDINSTANCE_H
#define FACTS_TOOL_STORAGE_RECORDINSTANCE_H

#include "model/RecordInstance.h"
#include "storage/Storage.h"

#include <expected>
#include <system_error>

namespace facts {

template <>
std::expected<SymbolId, std::error_code>
Storage::save<RecordInstance>(const RecordInstance &record);

} // namespace facts

#endif // FACTS_TOOL_STORAGE_RECORDINSTANCE_H
