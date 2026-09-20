#pragma once
#include "storage/FactProvenance.h"

namespace facts::storage {
// Refresh only files moving between explicitly registered clones, inside the
// caller's transaction. Unselected files and historical analysis runs remain.
std::expected<void, std::error_code>
refreshCloneFiles(Database &database, std::span<const FactProvenance> rows,
                   std::span<const FileId> selected);
}
