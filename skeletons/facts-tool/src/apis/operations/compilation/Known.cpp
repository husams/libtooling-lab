#include "apis/operations/compilation/Candidates.h"
#include "apis/operations/Details.h"
#include "storage/SqliteDatabase.h"

namespace facts::apis::operations::compilation {
domain::Result<std::set<FileId>> knownIncluders(const domain::ResolvedFile &file) {
  std::error_code error;
  if (!std::filesystem::exists(file.facts, error)) {
    if (error) return std::unexpected(storageError(error.message()));
    return std::set<FileId>{};
  }
  return storage::Database::open(file.facts.string())
      .transform_error([](auto failure) { return storageError(failure.message()); })
      .transform([&](storage::Database database) {
        std::set<FileId> result;
        bool present = false;
        for (const auto &row : database.rows("SELECT 1 FROM sqlite_master "
             "WHERE type='table' AND name='include_dependency'")) {
          (void)row;
          present = true;
        }
        if (!present) return result;
        for (const auto &row : database.rows(
             "WITH RECURSIVE includers(id) AS (VALUES(?1) UNION "
             "SELECT src_file_id FROM include_dependency JOIN includers "
             "ON dst_file_id=includers.id) SELECT id FROM includers", file.fileId))
          result.insert(static_cast<FileId>(row.integer(0)));
        return result;
      });
}
}
