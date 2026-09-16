#include "Sandbox.h"
#include <future>
#include <sqlite3.h>

namespace configuration_test {
void ownership() {
  Sandbox box;
  facts::config::Resolved value;
  value.projectRoot = box.root;
  value.storageRoot = box.root / "store";
  value.templateText = "same.db";
  value.database = *facts::config::renderDatabasePath(value);
  auto one = std::async(std::launch::async, [&] { return facts::config::ensureOwnedDatabase(value); });
  auto two = std::async(std::launch::async, [&] { return facts::config::ensureOwnedDatabase(value); });
  assert(one.get());
  assert(two.get());
  assert(facts::config::ensureOwnedDatabase(value));
  value.projectRoot = box.root / "other";
  assert(facts::config::ensureOwnedDatabase(value));
  assert(facts::config::ensureOwnedDatabase(value));
  sqlite3 *db = nullptr;
  assert(sqlite3_open(value.database.c_str(), &db) == SQLITE_OK);
  sqlite3_stmt *statement = nullptr;
  assert(sqlite3_prepare_v2(db, "SELECT count(*) FROM generated_conf_owner",
                            -1, &statement, nullptr) == SQLITE_OK);
  assert(sqlite3_step(statement) == SQLITE_ROW);
  assert(sqlite3_column_int(statement, 0) == 2);
  sqlite3_finalize(statement);
  sqlite3_close(db);
  value.templateText = "unowned.db";
  value.database = *facts::config::renderDatabasePath(value);
  db = nullptr;
  assert(sqlite3_open(value.database.c_str(), &db) == SQLITE_OK);
  assert(sqlite3_exec(db, "CREATE TABLE original(value)", nullptr, nullptr, nullptr) == SQLITE_OK);
  sqlite3_close(db);
  assert(!facts::config::ensureOwnedDatabase(value));
}
}
