#include "apis/index/Internal.h"

namespace facts::apis::index {
Result<bool> hasStateCounts(Database &database) {
  return catalog::query(database,
      "SELECT count(*)=2 FROM pragma_table_info('global_symbol_index_state') "
      "WHERE name IN ('sources','missing_sources')",
      [](const storage::Row &row) { return row.integer(0) != 0; })
      .and_then([](auto rows) { return catalog::requireOne(std::move(rows), "index metadata"); });
}
Result<void> ensureStateCounts(Database &database) {
  for (const std::string column : {"sources", "missing_sources"}) {
    auto existing = catalog::query(database,
        "SELECT count(*) FROM pragma_table_info('global_symbol_index_state') WHERE name=?",
        [](const storage::Row &row) { return row.integer(0); }, column);
    if (!existing) return std::unexpected(existing.error());
    if (existing->at(0) != 0) continue;
    auto result = catalog::execute(database,
        "ALTER TABLE global_symbol_index_state ADD COLUMN " + column +
        " INTEGER NOT NULL DEFAULT 0");
    if (!result) return result;
  }
  return {};
}
}
