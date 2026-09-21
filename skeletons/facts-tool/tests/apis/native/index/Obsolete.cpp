#include "Fixture.h"
#include <fstream>

namespace index_test {
void verifyObsoleteSources() {
  Fixture fixture;
  execute(fixture.first,
      "INSERT INTO symbol VALUES(4294967298,'removed','alpha::removed',13,1)");
  execute(fixture.second,
      "INSERT INTO symbol VALUES(4294967298,'removed','alpha::removed',13,1)");
  assert(index::refresh(fixture.project));
  const auto query = index::Query{.qualifiedName = "alpha::removed"};
  auto original = index::search(fixture.project, query);
  assert(original && original->items.size() == 1);
  // The second database remains registered for beta, but its obsolete alpha
  // copy cannot override the authoritative database recorded for alpha's file.
  execute(fixture.first, "DELETE FROM symbol WHERE usr='removed'");
  assert(index::refresh(fixture.project));
  auto removed = index::search(fixture.project, query);
  assert(removed && removed->items.empty());
  // Removing a definition exposes declarations from an unchanged source cache.
  execute(fixture.first, "DELETE FROM definition; DELETE FROM symbol WHERE usr='defined'");
  auto changed = index::refresh(fixture.project);
  assert(changed && changed->processedSources == 1 && changed->skippedSources == 1);
  auto declaration = index::search(fixture.project, {.qualifiedName = "alpha::defined"});
  assert(declaration && declaration->items.size() == 1 && !declaration->items[0].definition);
  fs::remove(fixture.first);
  assert(index::refresh(fixture.project));
  auto missing = index::search(fixture.project, query);
  assert(missing && missing->items.empty());
  // A later corrupt source rolls back earlier cache updates in the same job.
  Fixture recovery;
  assert(index::refresh(recovery.project));
  fs::copy_file(recovery.second, recovery.root / "second-backup.db");
  execute(recovery.first, "UPDATE symbol SET qualified_name='alpha::recovered' WHERE usr='shared'");
  std::ofstream(recovery.second, std::ios::binary | std::ios::trunc) << "corrupt database";
  assert(!index::refresh(recovery.project));
  auto unchanged = index::search(recovery.project, {.qualifiedName = "shared::same"});
  assert(unchanged && unchanged->items.size() == 2);
  fs::copy_file(recovery.root / "second-backup.db", recovery.second, fs::copy_options::overwrite_existing);
  auto retried = index::refresh(recovery.project);
  assert(retried && retried->processedSources == 2);
  auto recovered = index::search(recovery.project, {.qualifiedName = "alpha::recovered"});
  assert(recovered && recovered->items.size() == 1);
  // A new repository scope must refresh identity even when no facts changed.
  const auto oldIdentity = recovered->items[0].symbolId;
  execute(recovery.project, "UPDATE component SET repository_id=2,path='moved' WHERE id=2");
  auto moved = index::refresh(recovery.project);
  assert(moved && moved->processedSources == 0 && moved->generation > retried->generation);
  recovered = index::search(recovery.project, {.qualifiedName = "alpha::recovered"});
  assert(recovered && recovered->items[0].symbolId != oldIdentity);
}
}
