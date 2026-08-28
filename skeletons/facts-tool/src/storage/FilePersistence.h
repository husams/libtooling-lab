#ifndef FACTS_TOOL_STORAGE_FILE_PERSISTENCE_H
#define FACTS_TOOL_STORAGE_FILE_PERSISTENCE_H

#include "storage/FileIdentity.h"

#include "model/SymbolId.h"

#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <system_error>

struct sqlite3;

namespace facts {

std::expected<std::int64_t, std::error_code> fileRowCount(sqlite3 *database);

std::expected<std::int64_t, std::error_code>
upsertDirectory(sqlite3 *database, std::int64_t componentId,
                std::string_view directory);

std::expected<FileId, std::error_code>
persistFile(sqlite3 *database, const FileIdentity &identity);

std::expected<void, std::error_code>
persistFiles(sqlite3 *database, std::span<const FileIdentity> identities);

std::expected<FileId, std::error_code>
selectFileId(sqlite3 *database, const FileIdentity &identity);

std::expected<void, std::error_code>
insertFileWithId(sqlite3 *database, FileId id, const FileIdentity &identity);

} // namespace facts

#endif // FACTS_TOOL_STORAGE_FILE_PERSISTENCE_H
