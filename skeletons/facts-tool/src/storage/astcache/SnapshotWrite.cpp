#include "storage/astcache/Connection.h"

namespace facts::storage::astcache {
namespace {

using Snapshot = facts::astcache::Snapshot;
using detail::Result;

Result<void> upsert(storage::Database &database, const Snapshot &snapshot) {
  return catalog::execute(database,
      "DELETE FROM ast_cache_artifact WHERE snapshot_key=? AND generation<>?",
      snapshot.key, snapshot.generation)
      .and_then([&] {
        return catalog::execute(database,
            "INSERT INTO ast_cache_snapshot(key,source,working_directory,generation) "
            "VALUES(?,?,?,?) ON CONFLICT(key) DO UPDATE SET source=excluded.source,"
            "working_directory=excluded.working_directory,generation=excluded.generation",
            snapshot.key, snapshot.source, snapshot.working_directory, snapshot.generation);
      });
}

Result<void> inputs(storage::Database &database, const Snapshot &snapshot) {
  return catalog::execute(database, "DELETE FROM ast_cache_input WHERE snapshot_key=?", snapshot.key)
      .and_then([&] {
        return database.executeBulk("INSERT INTO ast_cache_input(snapshot_key,path) VALUES(?,?)",
            snapshot.inputs, [&](sqlite3_stmt *statement, const auto &value) {
              return bindParameters(statement, snapshot.key, value.path);
            }, {.atomic = false})
            .transform_error([&](auto) { return catalog::databaseError(database); })
            .transform([](auto) {});
      });
}

Result<void> includes(storage::Database &database, const Snapshot &snapshot) {
  return catalog::execute(database, "DELETE FROM ast_cache_include WHERE snapshot_key=?", snapshot.key)
      .and_then([&] {
        return database.executeBulk("INSERT INTO ast_cache_include(snapshot_key,source,target) VALUES(?,?,?)",
            snapshot.includes, [&](sqlite3_stmt *statement, const auto &value) {
              return bindParameters(statement, snapshot.key, value.source, value.target);
            }, {.atomic = false})
            .transform_error([&](auto) { return catalog::databaseError(database); })
            .transform([](auto) {});
      });
}

Result<void> revisions(storage::Database &database, const Snapshot &snapshot) {
  return catalog::execute(database, "DELETE FROM ast_cache_revision WHERE snapshot_key=?", snapshot.key)
      .and_then([&] {
        return database.executeBulk("INSERT INTO ast_cache_revision(snapshot_key,path,commit_hash) VALUES(?,?,?)",
            snapshot.revisions, [&](sqlite3_stmt *statement, const auto &value) {
              return bindParameters(statement, snapshot.key, value.path, value.commit);
            }, {.atomic = false})
            .transform_error([&](auto) { return catalog::databaseError(database); })
            .transform([](auto) {});
      });
}

} // namespace

Result<void> detail::storeSnapshot(storage::Database &database, const Snapshot &snapshot) {
  return upsert(database, snapshot)
      .and_then([&] { return inputs(database, snapshot); })
      .and_then([&] { return includes(database, snapshot); })
      .and_then([&] { return revisions(database, snapshot); });
}

Result<void> writeSnapshot(const std::filesystem::path &project, const Snapshot &snapshot) {
  return detail::open(project, true).and_then([&](storage::Database database) {
    return detail::transact(database, true, [&] { return detail::storeSnapshot(database, snapshot); });
  });
}

} // namespace facts::storage::astcache
