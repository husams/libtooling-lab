#include "Sandbox.h"
#include <future>
#include <sqlite3.h>

namespace configuration_test {
void database() {
  Sandbox box;
  facts::config::Resolved value;
  value.projectRoot = box.root;
  value.storageRoot = box.root / "store";
  value.templateText = "same.db";
  value.database = *facts::config::renderDatabasePath(value);
  auto one = std::async(std::launch::async, [&] { return facts::config::prepareDatabase(value); });
  auto two = std::async(std::launch::async, [&] { return facts::config::prepareDatabase(value); });
  assert(one.get());
  assert(two.get());
  assert(facts::config::prepareDatabase(value));
  value.projectRoot = box.root / "other";
  assert(facts::config::prepareDatabase(value));
  assert(facts::config::prepareDatabase(value));
  sqlite3 *db = nullptr;
  assert(sqlite3_open(value.database.c_str(), &db) == SQLITE_OK);
  sqlite3_stmt *statement = nullptr;
  assert(sqlite3_prepare_v2(db, "SELECT count(*) FROM sqlite_master "
                            "WHERE name='generated_conf_owner'",
                            -1, &statement, nullptr) == SQLITE_OK);
  assert(sqlite3_step(statement) == SQLITE_ROW);
  assert(sqlite3_column_int(statement, 0) == 0);
  sqlite3_finalize(statement);
  sqlite3_close(db);
  value.templateText = "unrelated.db";
  value.database = *facts::config::renderDatabasePath(value);
  db = nullptr;
  assert(sqlite3_open(value.database.c_str(), &db) == SQLITE_OK);
  assert(sqlite3_exec(db, "CREATE TABLE original(value)", nullptr, nullptr, nullptr) == SQLITE_OK);
  sqlite3_close(db);
  assert(!facts::config::prepareDatabase(value));

  // An interrupted old initialization may have left only the obsolete table.
  value.templateText = "partial.db";
  value.database = *facts::config::renderDatabasePath(value);
  assert(sqlite3_open(value.database.c_str(), &db) == SQLITE_OK);
  assert(sqlite3_exec(db, "CREATE TABLE generated_conf_owner(project_root TEXT PRIMARY KEY);"
                         "INSERT INTO generated_conf_owner VALUES('/old/checkout');",
                      nullptr, nullptr, nullptr) == SQLITE_OK);
  sqlite3_close(db);
  assert(facts::config::prepareDatabase(value));
  assert(sqlite3_open(value.database.c_str(), &db) == SQLITE_OK);
  assert(sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master "
                               "WHERE name IN ('generated_conf_owner','file')",
                            -1, &statement, nullptr) == SQLITE_OK);
  assert(sqlite3_step(statement) == SQLITE_ROW);
  assert(std::string(reinterpret_cast<const char *>(sqlite3_column_text(statement, 0))) == "file");
  assert(sqlite3_step(statement) == SQLITE_DONE);
  sqlite3_finalize(statement);
  sqlite3_close(db);
}
}
