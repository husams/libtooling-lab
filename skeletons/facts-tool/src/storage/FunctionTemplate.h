#ifndef FACTS_TOOL_STORAGE_FUNCTIONTEMPLATE_H
#define FACTS_TOOL_STORAGE_FUNCTIONTEMPLATE_H

#include "model/FunctionTemplate.h"
#include "storage/Storage.h"

#include <expected>
#include <system_error>

namespace facts {

template <>
std::expected<SymbolId, std::error_code>
Storage::save<FunctionTemplate>(const FunctionTemplate &function);

} // namespace facts

#endif // FACTS_TOOL_STORAGE_FUNCTIONTEMPLATE_H
