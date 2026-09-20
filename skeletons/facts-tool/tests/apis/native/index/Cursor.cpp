#include "Fixture.h"

namespace index_test {
void verifyLongCursor() {
  Fixture fixture;
  const std::string usr(20000, 'T');
  auto first = facts::storage::Database::open(fixture.first.string(),
      facts::storage::Database::readWrite);
  auto second = facts::storage::Database::open(fixture.second.string(),
      facts::storage::Database::readWrite);
  assert(first && second);
  for (auto *database : {&*first, &*second})
    assert(facts::catalog::execute(*database,
        "UPDATE symbol SET usr=? WHERE qualified_name='shared::same'", usr));
  assert(index::refresh(fixture.project));
  index::Query query{.qualifiedName = "shared::same", .limit = 1};
  auto page = index::search(fixture.project, query);
  assert(page && page->items.size() == 1 && page->nextCursor);
  assert(page->items[0].usr == usr && page->nextCursor->size() < 256);
  query.cursor = page->nextCursor;
  auto next = index::search(fixture.project, query);
  assert(next && next->items.size() == 1 && !next->nextCursor);
  assert(next->items[0].usr == usr && next->items[0].fileId != page->items[0].fileId);
}
}
