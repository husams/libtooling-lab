#include "../../astcache/Fixture.h"
#include <chrono>
#include <fstream>

int main() {
  using namespace ast_cache_database_test;
  Fixture fixture;
  const auto missing = fixture.root / "missing.db";
  assert(cache::clearSnapshots(missing));
  assert(!fs::exists(missing));
  assert(!cache::clearSnapshots(fixture.root));

  auto snapshot = fixture.snapshot();
  for (const auto *key : {"default-command", "alternate-command"}) {
    snapshot.key = key;
    const auto artifact = fixture.artifact(snapshot);
    assert(cache::writeArtifact(fixture.path, snapshot, artifact));
    std::ofstream(artifact.path) << "retained serialized AST";
  }
  assert(fixture.number("SELECT count(*) FROM ast_cache_snapshot") == 2);
  assert(fixture.number("SELECT count(*) FROM ast_cache_artifact") == 2);

  // Contention must return a bounded error without discarding cached evidence.
  auto blocker = fixture.open();
  auto transaction = blocker.write();
  assert(transaction);
  const auto start = std::chrono::steady_clock::now();
  assert(!cache::clearSnapshots(fixture.path));
  assert(std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
  assert(fixture.number("SELECT count(*) FROM ast_cache_snapshot") == 2);
  assert(transaction->rollback());

  assert(cache::clearSnapshots(fixture.path));
  for (const auto *table : {"ast_cache_snapshot", "ast_cache_input",
                          "ast_cache_include", "ast_cache_revision",
                          "ast_cache_artifact"})
    assert(fixture.number(std::string("SELECT count(*) FROM ") + table) == 0);
  assert(fixture.number("SELECT count(*) FROM file") == 1);
  assert(fixture.number("SELECT count(*) FROM pragma_foreign_key_check") == 0);
  for (const auto *key : {"default-command", "alternate-command"}) {
    const auto cached = cache::readSnapshot(fixture.path, key);
    const auto artifact = cache::readArtifact(fixture.path, key);
    assert(cached && !*cached && artifact && !*artifact);
  }
  assert(fs::exists(fixture.artifact(snapshot).path));
  assert(cache::clearSnapshots(fixture.path));

  // Refuse unsupported schemas without mutating existing metadata.
  assert(cache::writeSnapshot(fixture.path, snapshot));
  fixture.sql("UPDATE project_registry SET schema_version=999 WHERE id=1");
  assert(!cache::clearSnapshots(fixture.path));
  assert(fixture.number("SELECT count(*) FROM ast_cache_snapshot") == 1);
}
