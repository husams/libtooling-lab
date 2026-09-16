#include "storage/astcache/Connection.h"

namespace facts::storage::astcache {
namespace {

using Snapshot = facts::astcache::Snapshot;
using detail::Result;

Result<Snapshot> inputs(storage::Database &database, Snapshot snapshot) {
  return catalog::query(database,
      "SELECT path FROM ast_cache_input WHERE snapshot_key=? ORDER BY path",
      [](const Row &row) { return facts::astcache::Input{row.string(0)}; }, snapshot.key)
      .transform([&](auto values) {
        snapshot.inputs = std::move(values);
        return std::move(snapshot);
      });
}

Result<Snapshot> includes(storage::Database &database, Snapshot snapshot) {
  return catalog::query(database,
      "SELECT source,target FROM ast_cache_include WHERE snapshot_key=? ORDER BY source,target",
      [](const Row &row) { return facts::astcache::Include{row.string(0), row.string(1)}; }, snapshot.key)
      .transform([&](auto values) {
        snapshot.includes = std::move(values);
        return std::move(snapshot);
      });
}

Result<Snapshot> revisions(storage::Database &database, Snapshot snapshot) {
  return catalog::query(database,
      "SELECT path,commit_hash FROM ast_cache_revision WHERE snapshot_key=? ORDER BY path",
      [](const Row &row) { return facts::astcache::Revision{row.string(0), row.string(1)}; }, snapshot.key)
      .transform([&](auto values) {
        snapshot.revisions = std::move(values);
        return std::move(snapshot);
      });
}

Result<std::optional<Snapshot>> load(storage::Database &database, std::string_view key) {
  return catalog::query(database,
      "SELECT key,source,working_directory,generation FROM ast_cache_snapshot WHERE key=?",
      [](const Row &row) {
        return Snapshot{.key = row.string(0), .source = row.string(1),
                        .working_directory = row.string(2), .generation = row.string(3)};
      }, std::string(key))
      .and_then([&](auto rows) -> Result<std::optional<Snapshot>> {
        if (rows.empty()) return std::optional<Snapshot>{};
        return inputs(database, std::move(rows.front()))
            .and_then([&](auto value) { return includes(database, std::move(value)); })
            .and_then([&](auto value) { return revisions(database, std::move(value)); })
            .transform([](auto value) { return std::optional<Snapshot>{std::move(value)}; });
      });
}

} // namespace

Result<std::optional<Snapshot>> readSnapshot(const std::filesystem::path &project,
                                             std::string_view key) {
  return detail::open(project, false).and_then([&](storage::Database database) {
    return detail::transact(database, false, [&] { return load(database, key); });
  });
}

} // namespace facts::storage::astcache
