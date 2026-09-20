#include "Fixture.h"
#include "apis/index/Internal.h"

namespace index_test {
void verifyMigration() {
  Fixture fixture;
  assert(index::refresh(fixture.project));
  execute(fixture.project, R"sql(
ALTER TABLE global_symbol_index RENAME TO positioned_index;
CREATE TABLE global_symbol_index (
 usr TEXT NOT NULL,qualified_name TEXT NOT NULL,kind TEXT NOT NULL,
 path TEXT NOT NULL,file_id INTEGER NOT NULL,is_definition INTEGER NOT NULL,
 PRIMARY KEY(usr,file_id)) WITHOUT ROWID;
INSERT INTO global_symbol_index
 SELECT usr,qualified_name,kind,path,file_id,is_definition FROM positioned_index;
DROP TABLE positioned_index;
ALTER TABLE global_symbol_index_state DROP COLUMN sources;
ALTER TABLE global_symbol_index_state DROP COLUMN missing_sources;
)sql");
  auto legacy = index::published(fixture.project);
  assert(legacy && *legacy && (**legacy).symbols == 3 && (**legacy).sources == 0);
  auto database = facts::catalog::open(fixture.project.string(), true);
  assert(database && index::initialize(*database));
  auto preserved = index::search(fixture.project, {.qualifiedName = "shared::same"});
  assert(preserved && preserved->items.size() == 2);
  auto rebuilt = index::refresh(fixture.project);
  assert(rebuilt && rebuilt->sources == 2 && rebuilt->symbols == 3);
}
}
