#include "Sandbox.h"
#include "storage/FileSchema.h"
#include "storage/ProjectSchema.h"

#include <future>
#include <iterator>
#include <sqlite3.h>

namespace configuration_test {
namespace {
void execute(const fs::path &path, const std::string &sql) {
  sqlite3 *database = nullptr;
  assert(sqlite3_open(path.c_str(), &database) == SQLITE_OK);
  assert(sqlite3_exec(database, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK);
  assert(sqlite3_close(database) == SQLITE_OK);
}

std::string contents(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void expectRejected(const facts::config::Resolved &value,
                    const std::string &reason) {
  const auto before = contents(value.database);
  const auto result = facts::config::ensureOwnedDatabase(value);
  assert(!result);
  assert(result.error().find(reason) != std::string::npos);
  assert(contents(value.database) == before);
}
} // namespace

void existingDatabase() {
  Sandbox box;
  facts::config::Resolved value;
  value.projectRoot = box.root / "repo-one";
  value.storageRoot = box.root;
  // These catalogs have never been selected through generated naming.
  for (const auto version : {-1, 0, 1, facts::currentProjectSchemaVersion}) {
    value.templateText = "project-" + std::to_string(version) + ".db";
    value.database = *facts::config::renderDatabasePath(value);
    execute(value.database, facts::fileSchemaSql);
    execute(value.database, version < 0
        ? "ALTER TABLE project_registry DROP COLUMN schema_version"
        : "UPDATE project_registry SET schema_version=" + std::to_string(version));
    execute(value.database, "INSERT INTO repository(id,name) VALUES(42,'retained')");
    if (version == 1)
      execute(value.database, "CREATE TABLE generated_conf_owner(project_root TEXT PRIMARY KEY)");
    auto other = value;
    other.projectRoot = box.root / "repo-two";
    auto one = std::async(std::launch::async, [&] {
      return facts::config::ensureOwnedDatabase(value);
    });
    auto two = std::async(std::launch::async, [&] {
      return facts::config::ensureOwnedDatabase(other);
    });
    assert(one.get());
    assert(two.get());
    assert(facts::config::ensureOwnedDatabase(value));
    // CHECK constraints make these assertions apply to the actual saved rows.
    execute(value.database,
        "CREATE TEMP TABLE assertions(value INTEGER CHECK(value=1));"
        "INSERT INTO assertions SELECT count(*)=2 FROM generated_conf_owner;"
        "INSERT INTO assertions SELECT count(*)=1 FROM repository "
        "WHERE id=42 AND name='retained';");
  }
  value.templateText = "future.db";
  value.database = *facts::config::renderDatabasePath(value);
  execute(value.database, facts::fileSchemaSql);
  execute(value.database, "UPDATE project_registry SET schema_version=" +
      std::to_string(facts::currentProjectSchemaVersion + 1));
  expectRejected(value, "unsupported-project-schema");

  value.templateText = "unrelated.db";
  value.database = *facts::config::renderDatabasePath(value);
  execute(value.database, "CREATE TABLE original(value); INSERT INTO original VALUES('keep')");
  expectRejected(value, "not a project configuration database");

  value.templateText = "malformed.db";
  value.database = *facts::config::renderDatabasePath(value);
  execute(value.database, facts::fileSchemaSql);
  execute(value.database, "ALTER TABLE file RENAME COLUMN driver TO unrelated");
  expectRejected(value, "not a project configuration database");

  value.templateText = "invalid.db";
  value.database = box.write(value.templateText, "This is not a SQLite database.");
  expectRejected(value, "not a database");
}
} // namespace configuration_test
