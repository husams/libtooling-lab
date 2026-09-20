#ifndef FACTS_TOOL_STORAGE_FACT_PROVENANCE_H
#define FACTS_TOOL_STORAGE_FACT_PROVENANCE_H

#include "model/SymbolId.h"

#include <expected>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace facts::storage {

class Database;

struct FactProvenance {
  FileId file = builtinFileId;
  std::string path;
  std::string universe;
  // The same logical file in another explicitly registered repository clone.
  std::vector<std::string> aliases;
};

// Retain compatible existing rows, including registered alternate-clone paths.
// Clone refresh changes those paths only when that file is actually re-extracted.
std::expected<void, std::error_code>
registerFactProvenance(Database &database, std::span<const FactProvenance> rows,
                       std::span<const FileId> selected = {});

} // namespace facts::storage

#endif // FACTS_TOOL_STORAGE_FACT_PROVENANCE_H
