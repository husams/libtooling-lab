#include "apis/index/Internal.h"
#include "apis/index/Tables.h"

namespace facts::apis::index {
Result<void> ensurePositions(Database &database) {
  return catalog::query(database,
      "SELECT count(*) FROM pragma_table_info('global_symbol_index') WHERE name='position'",
      [](const storage::Row &row) { return row.integer(0); })
      .and_then([&](const auto &rows) -> Result<void> {
        if (rows.at(0) != 0) return {};
        return database.write()
            .transform_error([&](auto) { return catalog::databaseError(database); })
            .and_then([&](storage::Transaction transaction) {
              return catalog::execute(database,
                  "ALTER TABLE global_symbol_index RENAME TO global_symbol_legacy")
                  .and_then([&] {
                    return storage::execute(database.nativeHandle(), symbolTableSql)
                        .transform_error([&](auto) { return catalog::databaseError(database); });
                  }).and_then([&] {
                    return catalog::execute(database,
                        "INSERT INTO global_symbol_index(usr,qualified_name,kind,path,file_id,is_definition) "
                        "SELECT usr,qualified_name,kind,path,file_id,is_definition "
                        "FROM global_symbol_legacy ORDER BY usr,file_id");
                  }).and_then([&] {
                    return catalog::execute(database, "DROP TABLE global_symbol_legacy");
                  }).and_then([&] {
                    return transaction.commit().transform_error(
                        [&](auto) { return catalog::databaseError(database); });
                  });
            });
      });
}
}
