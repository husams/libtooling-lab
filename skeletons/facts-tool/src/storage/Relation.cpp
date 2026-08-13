#include "storage/Storage.h"

#include "storage/Sqlite.h"

#include <sqlite3.h>

#include <algorithm>
#include <system_error>

namespace facts {

std::expected<void, std::error_code>
Storage::addRelations(std::span<const Relation> relations) {
  auto transaction = storage::Transaction::write(handle());
  if (!transaction) {
    return std::unexpected(transaction.error());
  }

  const auto insert = [&](const Relation &relation) {
    auto statement = storage::prepare(
        handle(),
        "INSERT INTO relation(source_id,destination_id,kind,position,flags,"
        "count) VALUES(?1,?2,?3,?4,?5,?6) ON CONFLICT(source_id,"
        "destination_id,kind,position) DO NOTHING");
    return statement &&
           storage::bindInteger(statement->get(), 1,
                                storage::packSymbolId(relation.source)) &&
           storage::bindInteger(statement->get(), 2,
                                storage::packSymbolId(relation.destination)) &&
           storage::bindInteger(statement->get(), 3, relation.kind) &&
           storage::bindInteger(statement->get(), 4, relation.position) &&
           storage::bindInteger(statement->get(), 5, relation.flags) &&
           storage::bindInteger(statement->get(), 6, relation.count) &&
           sqlite3_step(statement->get()) == SQLITE_DONE;
  };
  if (!std::ranges::all_of(relations, insert)) {
    return std::unexpected(storage::sqliteError(handle()));
  }

  return transaction->commit();
}

} // namespace facts
