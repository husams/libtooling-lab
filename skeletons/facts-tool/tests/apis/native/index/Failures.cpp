#include "Fixture.h"
#include <fstream>

namespace index_test {
void verifyFailures(Fixture &fixture) {
  fs::remove(fixture.second);
  auto missing = index::refresh(fixture.project);
  assert(missing && missing->symbols == 2 && missing->missingSources == 1);
  const auto query = index::Query{.qualifiedName = "shared::same"};
  auto snapshot = index::search(fixture.project, query);
  assert(snapshot && snapshot->items.size() == 1);
  std::ofstream(fixture.first, std::ios::binary | std::ios::trunc) << "corrupt database";
  assert(!index::refresh(fixture.project));
  auto preserved = index::search(fixture.project, query);
  assert(preserved && preserved->generation == snapshot->generation);
  assert(preserved->items.size() == 1 && preserved->items[0].usr == "shared");
  const auto restart = index::published(fixture.project);
  assert(restart && *restart && (**restart).generation == snapshot->generation);
  assert((**restart).sources == 1 && (**restart).symbols == 2);
  // The previous snapshot remains durable after every connection has closed.
  auto reopened = facts::catalog::open(fixture.project.string(), false);
  assert(reopened);
  auto counts = facts::catalog::query(*reopened,
      "SELECT count(*) FROM global_symbol_index",
      [](const facts::storage::Row &row) { return row.integer(0); });
  assert(counts && counts->at(0) == 2);
}
}
