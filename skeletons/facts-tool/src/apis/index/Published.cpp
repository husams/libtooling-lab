#include "apis/index/Internal.h"

namespace facts::apis::index {
Result<std::optional<RefreshResult>> published(const std::filesystem::path &project) {
  return catalog::open(project.string(), false).and_then([](Database database) {
    return catalog::query(database,
        "SELECT count(*) FROM sqlite_master WHERE type='table' "
        "AND name='global_symbol_index_state'",
        [](const storage::Row &row) { return row.integer(0); })
        .and_then([&](const auto &present) -> Result<std::optional<RefreshResult>> {
          if (present.at(0) == 0) return std::nullopt;
          return hasStateCounts(database).and_then([&](bool counts) {
            const std::string fields = counts ? "sources,missing_sources" : "0,0";
            return catalog::query(database,
                "SELECT generation,(SELECT count(*) FROM global_symbol_index)," + fields +
                " FROM global_symbol_index_state WHERE id=1 AND generation>0",
                [](const storage::Row &row) {
                  return RefreshResult{row.integer(0), row.integer(1),
                      row.get<std::size_t>(2), row.get<std::size_t>(3)};
                }).transform([](auto rows) -> std::optional<RefreshResult> {
                  return rows.empty() ? std::nullopt : std::optional{rows.front()};
                });
          });
        });
  });
}
}
