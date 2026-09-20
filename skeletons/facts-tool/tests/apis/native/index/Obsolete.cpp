#include "Fixture.h"

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
  fs::remove(fixture.first);
  assert(index::refresh(fixture.project));
  auto missing = index::search(fixture.project, query);
  assert(missing && missing->items.empty());
}
}
