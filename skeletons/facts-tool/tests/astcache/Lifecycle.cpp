#include "Fixture.h"

namespace ast_cache_database_test {

void lifecycle() {
  Fixture fixture;
  auto snapshot = fixture.snapshot();
  auto absent = cache::readSnapshot(fixture.path, snapshot.key);
  assert(absent && !*absent);
  assert(cache::writeSnapshot(fixture.path, snapshot));
  auto loaded = cache::readSnapshot(fixture.path, snapshot.key);
  assert(loaded && *loaded);
  assert((*loaded)->source == snapshot.source);
  assert((*loaded)->working_directory == snapshot.working_directory);
  assert((*loaded)->generation == snapshot.generation);
  assert((*loaded)->inputs == snapshot.inputs);
  assert((*loaded)->includes == snapshot.includes);
  assert((*loaded)->revisions == snapshot.revisions);
  auto missingArtifact = cache::readArtifact(fixture.path, snapshot.key);
  assert(missingArtifact && !*missingArtifact);

  const auto artifact = fixture.artifact(snapshot);
  assert(cache::writeArtifact(fixture.path, snapshot, artifact));
  assert(cache::writeSnapshot(fixture.path, snapshot));
  auto retained = cache::readArtifact(fixture.path, snapshot.key);
  assert(retained && *retained && (*retained)->digest == artifact.digest);
  assert((*retained)->path == artifact.path && (*retained)->generation == snapshot.generation);
  assert(fixture.number("SELECT count(*) FROM ast_cache_snapshot") == 1);
  assert(fixture.number("SELECT count(*) FROM ast_cache_input") == 2);
  assert(fixture.number("SELECT count(*) FROM ast_cache_include") == 1);
  assert(fixture.number("SELECT count(*) FROM ast_cache_revision") == 1);

  snapshot.generation = "generation-two";
  snapshot.revisions.front().commit = "commit-two";
  assert(cache::writeSnapshot(fixture.path, snapshot));
  auto invalidated = cache::readArtifact(fixture.path, snapshot.key);
  assert(invalidated && !*invalidated);
  assert(!cache::writeArtifact(fixture.path, snapshot, artifact));
  assert(cache::writeArtifact(fixture.path, snapshot, fixture.artifact(snapshot)));
  assert(fixture.number("SELECT count(*) FROM pragma_foreign_key_check") == 0);
}

} // namespace ast_cache_database_test
