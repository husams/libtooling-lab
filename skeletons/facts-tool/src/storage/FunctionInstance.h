#ifndef FACTS_TOOL_STORAGE_FUNCTIONINSTANCE_H
#define FACTS_TOOL_STORAGE_FUNCTIONINSTANCE_H

#include "model/FunctionInstance.h"
#include "storage/Storage.h"

#include <expected>
#include <system_error>

namespace facts {

template <>
std::expected<SymbolId, std::error_code>
Storage::save<FunctionInstance>(const FunctionInstance &function);

} // namespace facts

#endif // FACTS_TOOL_STORAGE_FUNCTIONINSTANCE_H
