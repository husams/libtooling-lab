#include "Fixture.h"
#include <algorithm>

namespace index_test {
void verifySnapshot(Fixture &fixture) {
  const auto absent = index::published(fixture.project);
  assert(absent && !*absent);
  auto first = index::refresh(fixture.project);
  assert(first && first->sources == 2 && first->symbols == 3);
  const auto durable = index::published(fixture.project);
  assert(durable && *durable && (**durable).sources == 2 && (**durable).symbols == 3);
  index::Query query{.qualifiedName = "shared::same", .limit = 1};
  auto page = index::search(fixture.project, query);
  assert(page && page->items.size() == 1 && page->nextCursor);
  assert(page->items[0].fileId == 1 && page->items[0].clone == "inactive");
  query.cursor = page->nextCursor;
  auto next = index::search(fixture.project, query);
  assert(next && next->items.size() == 1 && !next->nextCursor);
  assert(next->items[0].fileId == 2 && next->items[0].repository == "beta");
  auto rebuilt = index::refresh(fixture.project);
  assert(rebuilt && rebuilt->symbols == first->symbols);
  assert(rebuilt->generation > first->generation);
  assert(!index::search(fixture.project, query));
  const auto defined = index::search(fixture.project, {.qualifiedName = "alpha::defined"});
  assert(defined && defined->items.size() == 1);
  assert(defined->items[0].fileId == 1 && defined->items[0].definition);
  assert(defined->items[0].path == "/other-alpha/a.cpp");
  auto database = facts::catalog::open(fixture.project.string(), false);
  assert(database);
  auto plan = facts::catalog::query(*database,
      "EXPLAIN QUERY PLAN SELECT usr,file_id FROM global_symbol_index "
      "WHERE qualified_name='shared::same' ORDER BY position",
      [](const facts::storage::Row &row) { return row.string(3); });
  assert(plan && std::ranges::any_of(*plan, [](const auto &detail) {
    return detail.find("USING COVERING INDEX global_symbol_name") != std::string::npos;
  }));
  // A resolver-provided list is authoritative for configured relative paths.
  const auto base = fixture.root / "configured";
  fs::create_directory(base);
  fs::create_symlink(fixture.first, base / "relative.db");
  execute(fixture.project, "UPDATE file SET facts_db='relative.db' WHERE id=1");
  execute(fixture.root / "relative.db", "CREATE TABLE unrelated(value)");
  const auto configured = index::refresh(fixture.project, {fixture.first, fixture.second}, base);
  assert(configured && configured->sources == 2 && configured->symbols == 3);
  auto writable = facts::catalog::open(fixture.project.string(), true);
  assert(writable);
  assert(facts::catalog::execute(*writable,
      "UPDATE file SET facts_db=? WHERE id=1", fixture.first.string()));
}
}
