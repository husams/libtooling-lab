#pragma once
#include "model/SymbolId.h"
#include <expected>
#include <span>
#include <system_error>
namespace facts::storage {
class Database;
std::expected<void, std::error_code>
beginSymbolRefresh(Database &database, std::span<const FileId> selected);
std::expected<void, std::error_code> finishSymbolRefresh(Database &database);
}
