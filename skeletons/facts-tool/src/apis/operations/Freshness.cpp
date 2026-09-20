#include "apis/operations/Details.h"
#include "storage/SqliteDatabase.h"

namespace facts::apis::operations {
domain::Result<bool> needsExtraction(const domain::ResolvedFile &file) {
  std::error_code error;
  if (!std::filesystem::exists(file.facts, error)) {
    if (error) return std::unexpected(storageError(error.message()));
    return true;
  }
  return storage::Database::open(file.facts.string())
      .transform_error([](auto error) { return storageError(error.message()); })
      .transform([&](storage::Database database) {
        bool hasProvenance = false;
        for (const auto &row : database.rows(
                 "SELECT 1 FROM sqlite_master WHERE type='table' "
                 "AND name='facts_project_provenance'")) {
          (void)row;
          hasProvenance = true;
        }
        if (!hasProvenance) return true;
        for (const auto &row : database.rows(
                 "SELECT path FROM facts_project_provenance WHERE file_id=?1",
                 file.fileId))
          return std::filesystem::path(row.text(0)).lexically_normal() !=
                 file.path.lexically_normal();
        return true;
      });
}
}
