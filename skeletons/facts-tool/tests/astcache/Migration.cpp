#include "Fixture.h"
#include "storage/ProjectSchema.h"

namespace ast_cache_database_test {
namespace {

auto registryRows(facts::storage::Database &database) {
  return facts::catalog::query(database,
      "SELECT f.id,f.compile_options,f.working_directory,f.directory_id,c.id,r.id "
      "FROM file f JOIN directory d ON d.id=f.directory_id "
      "JOIN component c ON c.id=d.component_id JOIN repository r ON r.id=c.repository_id",
      [](const facts::storage::Row &row) {
        std::vector<std::string> values;
        for (int index = 0; index < row.columns(); ++index) values.push_back(row.string(index));
        return values;
      });
}

} // namespace

void migration() {
  Fixture fixture;
  auto database = fixture.open();
  auto before = registryRows(database);
  assert(before && before->size() == 1);
  assert(database.executeScript(R"sql(
DROP TABLE ast_cache_artifact;
DROP TABLE ast_cache_revision;
DROP TABLE ast_cache_include;
DROP TABLE ast_cache_input;
DROP TABLE ast_cache_snapshot;
UPDATE project_registry SET schema_version=1 WHERE id=1;
)sql"));
  auto legacyRead = cache::readSnapshot(fixture.path, "missing");
  assert(!legacyRead && legacyRead.error().find("re-run 'facts-tool import'") != std::string::npos);
  assert(!cache::writeSnapshot(fixture.path, fixture.snapshot()));
  assert(fixture.number("SELECT schema_version FROM project_registry") == 1);
  assert(facts::migrateProjectSchema(database.nativeHandle()));
  assert(fixture.number("SELECT schema_version FROM project_registry") == 2);
  assert(registryRows(database) == before);
  assert(cache::writeSnapshot(fixture.path, fixture.snapshot()));
  assert(facts::migrateProjectSchema(database.nativeHandle()));
  assert(fixture.number("SELECT count(*) FROM ast_cache_snapshot") == 1);
  assert(registryRows(database) == before);

  assert(database.executeScript("UPDATE project_registry SET schema_version=999 WHERE id=1"));
  auto newer = facts::migrateProjectSchema(database.nativeHandle());
  assert(!newer && newer.error().find("unsupported-project-schema") != std::string::npos);
  assert(fixture.number("SELECT schema_version FROM project_registry") == 999);
}

} // namespace ast_cache_database_test
