#include "Fixture.h"
#include <cstdlib>
#include <iostream>

namespace index_test {
void execute(const fs::path &path, const std::string &sql) {
  auto database = facts::storage::Database::open(path.string(),
      facts::storage::Database::readWrite);
  assert(database);
  const auto result = facts::storage::execute(database->nativeHandle(), sql);
  if (!result)
    std::cerr << "Fixture SQL failed in " << path << ": "
              << sqlite3_errmsg(database->nativeHandle()) << "\n" << sql << "\n";
  assert(result);
}
Fixture::Fixture() {
  auto pattern = (fs::temp_directory_path() / "facts-index-XXXXXX").string();
  const auto created = ::mkdtemp(pattern.data());
  assert(created);
  root = created;
  project = root / "project.db";
  first = root / "first.db";
  second = root / "second.db";
  assert(facts::catalog::open(project.string(), true, true));
  execute(project, R"sql(
INSERT INTO repository(id,name,active_clone_id) VALUES(1,'alpha',1),(2,'beta',2);
INSERT INTO clone(id,repository_id,path,label) VALUES
 (1,1,'/alpha','active'),(2,2,'/beta','main'),(3,1,'/other-alpha','inactive');
INSERT INTO component(id,name,path,repository_id) VALUES(2,'a','.',1),(3,'b','.',2);
INSERT INTO directory(id,component_id,path) VALUES(1,2,'.'),(2,3,'.');
INSERT INTO file(id,directory_id,name) VALUES(1,1,'a.cpp'),(2,2,'b.cpp'),(3,1,'a.h');
)sql");
  for (const auto &path : {first, second})
    execute(path, R"sql(
CREATE TABLE symbol(id INTEGER PRIMARY KEY,usr TEXT,qualified_name TEXT,
 kind INTEGER,is_definition INTEGER);
CREATE TABLE definition(symbol_id INTEGER,file_id INTEGER);
CREATE TABLE facts_project_provenance(file_id INTEGER,path TEXT);
)sql");
  execute(first, R"sql(
INSERT INTO symbol VALUES(4294967297,'shared','shared::same',13,1);
INSERT INTO symbol VALUES(12884901889,'defined','alpha::defined',13,0);
INSERT INTO definition VALUES(12884901889,1);
INSERT INTO facts_project_provenance VALUES(1,'/other-alpha/a.cpp'),(3,'/other-alpha/a.h');
)sql");
  execute(second, R"sql(
INSERT INTO symbol VALUES(8589934593,'shared','shared::same',13,1);
INSERT INTO symbol VALUES(12884901890,'defined','alpha::defined',13,0);
INSERT INTO facts_project_provenance VALUES(2,'/beta/b.cpp');
INSERT INTO facts_project_provenance VALUES(3,'/other-alpha/a.h');
)sql");
  auto database = facts::catalog::open(project.string(), true);
  assert(database);
  assert(facts::catalog::execute(*database, "UPDATE file SET facts_db=? WHERE id=1", first.string()));
  assert(facts::catalog::execute(*database, "UPDATE file SET facts_db=? WHERE id=2", second.string()));
}
}
