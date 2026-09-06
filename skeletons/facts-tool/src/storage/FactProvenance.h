#ifndef FACTS_TOOL_STORAGE_FACT_PROVENANCE_H
#define FACTS_TOOL_STORAGE_FACT_PROVENANCE_H

#include "model/SymbolId.h"

#include <expected>
#include <span>
#include <string>
#include <system_error>

namespace facts::storage {

class Database;

struct FactProvenance {
  FileId file = builtinFileId;
  std::string path;
  std::string universe;
};

// Insert the rows selected by file id while retaining the caller's active
// transaction; an empty selection records no rows.
std::expected<void, std::error_code>
registerFactProvenance(Database &database, std::span<const FactProvenance> rows,
                       std::span<const FileId> selected = {});

} // namespace facts::storage

#endif // FACTS_TOOL_STORAGE_FACT_PROVENANCE_H
