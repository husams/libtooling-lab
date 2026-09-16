#include "Fixture.h"

#include <chrono>

namespace ast_cache_database_test {

void failures() {
  Fixture fixture;
  const auto snapshot = fixture.snapshot();
  assert(!cache::writeSnapshot(fixture.root / "missing.db", snapshot));
  assert(!fs::exists(fixture.root / "missing.db"));
  assert(!cache::readSnapshot(fixture.root / "missing.db", snapshot.key));
  assert(cache::writeArtifact(fixture.path, snapshot, fixture.artifact(snapshot)));

  auto invalid = snapshot;
  invalid.generation = "invalid-generation";
  invalid.inputs.push_back({""});
  assert(!cache::writeSnapshot(fixture.path, invalid));
  auto retained = cache::readSnapshot(fixture.path, snapshot.key);
  assert(retained && *retained && (*retained)->generation == snapshot.generation);
  auto artifact = cache::readArtifact(fixture.path, snapshot.key);
  assert(artifact && *artifact && (*artifact)->generation == snapshot.generation);

  auto invalidArtifact = fixture.artifact(snapshot);
  invalidArtifact.digest.clear();
  auto changed = snapshot;
  changed.includes.clear();
  assert(!cache::writeArtifact(fixture.path, changed, invalidArtifact));
  retained = cache::readSnapshot(fixture.path, snapshot.key);
  assert(retained && *retained && (*retained)->includes == snapshot.includes);

  auto blocker = fixture.open();
  auto transaction = blocker.write();
  assert(transaction);
  const auto start = std::chrono::steady_clock::now();
  assert(!cache::writeSnapshot(fixture.path, snapshot));
  assert(std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
  assert(transaction->rollback());
  assert(cache::writeSnapshot(fixture.path, snapshot));
}

} // namespace ast_cache_database_test
