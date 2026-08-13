#include "storage/Storage.h"

#include "storage/Sqlite.h"

#include <sqlite3.h>

namespace facts {

std::expected<std::optional<Region>, std::error_code>
Storage::loadDefinition(SymbolId id) {
  auto statement = storage::prepare(
      handle(), "SELECT offset,size FROM definition WHERE symbol_id=?1");
  if (!statement ||
      !storage::bindInteger(statement->get(), 1, storage::packSymbolId(id))) {
    return std::unexpected(storage::sqliteError(handle()));
  }

  const auto step = sqlite3_step(statement->get());
  if (step == SQLITE_DONE) {
    return std::optional<Region>{};
  }
  if (step != SQLITE_ROW) {
    return std::unexpected(storage::sqliteError(handle()));
  }
  return std::optional{Region{
      static_cast<unsigned>(sqlite3_column_int64(statement->get(), 0)),
      static_cast<unsigned>(sqlite3_column_int64(statement->get(), 1)),
  }};
}

std::expected<void, std::error_code>
Storage::replaceDefinition(SymbolId id,
                           const std::optional<Region> &definition) {
  auto clear =
      storage::prepare(handle(), "DELETE FROM definition WHERE symbol_id=?1");
  if (!clear ||
      !storage::bindInteger(clear->get(), 1, storage::packSymbolId(id)) ||
      sqlite3_step(clear->get()) != SQLITE_DONE) {
    return std::unexpected(storage::sqliteError(handle()));
  }
  if (!definition) {
    return {};
  }

  auto statement = storage::prepare(
      handle(), "INSERT INTO definition(symbol_id,file_id,offset,size) "
                "VALUES(?1,?2,?3,?4)");
  if (!statement ||
      !storage::bindInteger(statement->get(), 1, storage::packSymbolId(id)) ||
      !storage::bindInteger(statement->get(), 2, id.file) ||
      !storage::bindInteger(statement->get(), 3, definition->offset) ||
      !storage::bindInteger(statement->get(), 4, definition->size) ||
      sqlite3_step(statement->get()) != SQLITE_DONE) {
    return std::unexpected(storage::sqliteError(handle()));
  }
  return {};
}

} // namespace facts
