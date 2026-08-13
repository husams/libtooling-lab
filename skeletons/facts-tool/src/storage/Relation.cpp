#include "storage/Storage.h"

#include "storage/SemanticProperties.h"
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
    const auto properties = storage::relationProperties(relation.flags);
    auto statement = storage::prepare(
        handle(),
        "INSERT INTO relation(source_id,destination_id,kind,position,access,"
        "is_virtual_base,is_implicit,is_lexical,count) "
        "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9) ON CONFLICT(source_id,"
        "destination_id,kind,position) DO NOTHING");
    return statement &&
           storage::bindInteger(statement->get(), 1,
                                storage::packSymbolId(relation.source)) &&
           storage::bindInteger(statement->get(), 2,
                                storage::packSymbolId(relation.destination)) &&
           storage::bindInteger(statement->get(), 3, relation.kind) &&
           storage::bindInteger(statement->get(), 4, relation.position) &&
           storage::bindText(statement->get(), 5, properties.access) &&
           storage::bindInteger(statement->get(), 6,
                                properties.isVirtualBase) &&
           storage::bindInteger(statement->get(), 7, properties.isImplicit) &&
           storage::bindInteger(statement->get(), 8, properties.isLexical) &&
           storage::bindInteger(statement->get(), 9, relation.count) &&
           sqlite3_step(statement->get()) == SQLITE_DONE;
  };
  if (!std::ranges::all_of(relations, insert)) {
    return std::unexpected(storage::sqliteError(handle()));
  }

  return transaction->commit();
}

} // namespace facts
