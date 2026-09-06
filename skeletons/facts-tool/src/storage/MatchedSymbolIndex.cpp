#include "storage/MatchedSymbolIndex.h"

#include "storage/SqliteDatabase.h"

#include <sqlite3.h>

namespace facts::storage {

std::expected<void, std::error_code>
upsertMatchedSymbols(Database &database,
                     std::span<const MatchedSymbol> symbols) {
  constexpr auto sql =
      "INSERT INTO matched_symbol_index(usr,qualified_name,file_id,kind) "
      "VALUES(?1,?2,?3,?4) ON CONFLICT(usr,file_id) DO UPDATE SET "
      "qualified_name=excluded.qualified_name,kind=excluded.kind";
  return database
      .executeBulk(sql, symbols,
                   [](sqlite3_stmt *statement, const MatchedSymbol &symbol) {
                     return bindParameters(statement, symbol.usr,
                                           symbol.qualifiedName, symbol.fileId,
                                           symbol.kind);
                   },
                   {.atomic = false})
      .transform([](const BulkResult &) {});
}

} // namespace facts::storage
