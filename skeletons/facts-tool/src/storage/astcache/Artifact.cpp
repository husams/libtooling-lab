#include "storage/astcache/Connection.h"

namespace facts::storage::astcache {
namespace {

using Artifact = facts::astcache::Artifact;
using Snapshot = facts::astcache::Snapshot;
using detail::Result;

Result<std::optional<Artifact>> load(storage::Database &database, std::string_view key) {
  return catalog::query(database,
      "SELECT a.snapshot_key,a.path,a.digest,a.generation FROM ast_cache_artifact a "
      "JOIN ast_cache_snapshot s ON s.key=a.snapshot_key AND s.generation=a.generation "
      "WHERE a.snapshot_key=?",
      [](const Row &row) { return Artifact{row.string(0), row.string(1), row.string(2), row.string(3)}; },
      std::string(key))
      .transform([](auto rows) {
        return rows.empty() ? std::optional<Artifact>{}
                            : std::optional<Artifact>{std::move(rows.front())};
      });
}

Result<void> validate(const Snapshot &snapshot, const Artifact &artifact) {
  if (snapshot.key != artifact.key || snapshot.generation != artifact.generation)
    return std::unexpected("AST artifact must match the snapshot key and generation");
  return {};
}

Result<void> store(storage::Database &database, const Artifact &artifact) {
  return catalog::execute(database,
      "INSERT INTO ast_cache_artifact(snapshot_key,path,digest,generation) VALUES(?,?,?,?) "
      "ON CONFLICT(snapshot_key) DO UPDATE SET path=excluded.path,digest=excluded.digest,"
      "generation=excluded.generation",
      artifact.key, artifact.path, artifact.digest, artifact.generation);
}

} // namespace

Result<std::optional<Artifact>> readArtifact(const std::filesystem::path &project,
                                             std::string_view key) {
  return detail::open(project, false).and_then([&](storage::Database database) {
    return detail::transact(database, false, [&] { return load(database, key); });
  });
}

Result<void> writeArtifact(const std::filesystem::path &project,
                           const Snapshot &snapshot, const Artifact &artifact) {
  return validate(snapshot, artifact)
      .and_then([&] { return detail::open(project, true); })
      .and_then([&](storage::Database database) {
        return detail::transact(database, true, [&] {
          return detail::storeSnapshot(database, snapshot)
              .and_then([&] { return store(database, artifact); });
        });
      });
}

} // namespace facts::storage::astcache
