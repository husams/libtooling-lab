#include "apis/index/Internal.h"
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA256.h>
#include <nlohmann/json.hpp>

namespace facts::apis::index {
std::string symbolIdentity(const std::string &usr, std::int64_t scopeId,
                           bool repositoryScope) {
  llvm::SHA256 hash;
  hash.update(nlohmann::json::array({repositoryScope ? "repository" : "component",
                                    scopeId, usr}).dump());
  constexpr char digits[] = "0123456789abcdef";
  std::string result = "sym_";
  for (const auto byte : hash.final()) {
    result += digits[byte >> 4];
    result += digits[byte & 15];
  }
  return result;
}
Result<void> updateIdentities(Database &database) {
  return storage::prepareSingle(database.nativeHandle(),
      "UPDATE global_symbol_index SET symbol_id=? WHERE position=?")
      .transform_error([&](auto) { return catalog::databaseError(database); })
      .and_then([&](storage::Statement statement) -> Result<void> {
        try {
          for (const auto &row : database.rows(
              "SELECT i.position,i.usr,coalesce(c.repository_id,c.id),c.repository_id IS NOT NULL "
              "FROM global_symbol_index i NOT INDEXED JOIN file f ON f.id=i.file_id "
              "JOIN directory d ON d.id=f.directory_id JOIN component c ON c.id=d.component_id "
              "WHERE i.symbol_id='' ORDER BY i.position")) {
            const auto id = symbolIdentity(row.string(1), row.integer(2), row.integer(3) != 0);
            sqlite3_reset(statement.get());
            sqlite3_clear_bindings(statement.get());
            if (!storage::bindParameters(statement.get(), id, row.integer(0)) ||
                sqlite3_step(statement.get()) != SQLITE_DONE)
              return std::unexpected(catalog::databaseError(database));
          }
          return {};
        } catch (const storage::QueryError &error) { return std::unexpected(error.what()); }
      });
}
Result<void> ensureIdentities(Database &database) {
  return catalog::query(database,
      "SELECT count(*) FROM pragma_table_info('global_symbol_index') WHERE name='symbol_id'",
      [](const storage::Row &row) { return row.integer(0); })
      .and_then([&](const auto &rows) -> Result<void> {
        if (rows.at(0)) return {};
        return catalog::execute(database,
            "ALTER TABLE global_symbol_index ADD COLUMN symbol_id TEXT NOT NULL DEFAULT ''");
      }).and_then([&] {
        return database.write().transform_error([&](auto) { return catalog::databaseError(database); })
            .and_then([&](storage::Transaction transaction) {
              return updateIdentities(database).and_then([&] {
                return transaction.commit().transform_error([&](auto) { return catalog::databaseError(database); });
              });
            });
      })
      .and_then([&] {
        return catalog::execute(database,
            "CREATE INDEX IF NOT EXISTS global_symbol_id ON global_symbol_index"
            "(symbol_id,is_definition DESC,position)");
      }).and_then([&] {
        return catalog::execute(database,
            "CREATE INDEX IF NOT EXISTS global_symbol_usr ON global_symbol_index(usr,position)");
      });
}
}
