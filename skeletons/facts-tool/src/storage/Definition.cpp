#include "storage/Storage.h"

#include "storage/Sqlite.h"

#include <sqlite3.h>

namespace facts {

std::expected<std::optional<Storage::DefinitionFacts>, std::error_code>
Storage::loadDefinition(SymbolId id) {
  auto statement = storage::prepare(
      handle(),
      "SELECT file_id,offset,size FROM definition WHERE symbol_id=?1");
  if (!statement ||
      !storage::bindInteger(statement->get(), 1, storage::packSymbolId(id))) {
    return std::unexpected(storage::sqliteError(handle()));
  }

  const auto step = sqlite3_step(statement->get());
  if (step == SQLITE_DONE) {
    return std::optional<DefinitionFacts>{};
  }
  if (step != SQLITE_ROW) {
    return std::unexpected(storage::sqliteError(handle()));
  }
  return std::optional{DefinitionFacts{
      .file = static_cast<FileId>(sqlite3_column_int64(statement->get(), 0)),
      .region =
          Region{
              static_cast<unsigned>(sqlite3_column_int64(statement->get(), 1)),
              static_cast<unsigned>(sqlite3_column_int64(statement->get(), 2)),
          },
  }};
}

std::expected<void, std::error_code>
Storage::replaceDefinition(SymbolId id, FileId file,
                           const std::optional<Region> &definition) {
  if (!definition) {
    return {};
  }

  auto statement = storage::prepare(
      handle(), "INSERT INTO definition(symbol_id,file_id,offset,size) "
                "VALUES(?1,?2,?3,?4) "
                "ON CONFLICT(symbol_id) DO UPDATE SET file_id=excluded.file_id,"
                "offset=excluded.offset,size=excluded.size");
  if (!statement ||
      !storage::bindInteger(statement->get(), 1, storage::packSymbolId(id)) ||
      !storage::bindInteger(statement->get(), 2, file) ||
      !storage::bindInteger(statement->get(), 3, definition->offset) ||
      !storage::bindInteger(statement->get(), 4, definition->size) ||
      sqlite3_step(statement->get()) != SQLITE_DONE) {
    return std::unexpected(storage::sqliteError(handle()));
  }
  return {};
}

} // namespace facts
