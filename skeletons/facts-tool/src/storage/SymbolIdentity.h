#ifndef FACTS_TOOL_STORAGE_SYMBOL_IDENTITY_H
#define FACTS_TOOL_STORAGE_SYMBOL_IDENTITY_H

#include "model/Symbol.h"

#include <expected>
#include <string>
#include <string_view>
#include <system_error>

struct sqlite3;

namespace facts {

std::string symbolIdentity(const Symbol &symbol);

std::expected<SymbolId, std::error_code>
findOrAllocateSymbol(sqlite3 *database, FileId file, std::string_view identity);

} // namespace facts

#endif
