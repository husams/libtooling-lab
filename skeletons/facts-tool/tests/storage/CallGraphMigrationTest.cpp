#include "StorageSchemaTestCases.h"
#include "StorageSchemaTestSupport.h"

#include "storage/Storage.h"

#include <sqlite3.h>

namespace storage_schema_test {
namespace {

bool createVersionSeven(const std::filesystem::path &path,
                        std::int64_t &tableCount) {
  removeDatabase(path);
  {
    facts::Storage current{path.string()};
  }
  sqlite3 *database = nullptr;
  if (sqlite3_open(path.c_str(), &database) != SQLITE_OK)
    return false;
  tableCount =
      scalar(database, "SELECT COUNT(*) FROM sqlite_master WHERE type='table'");
  const auto created = execute(database, R"sql(
ALTER TABLE relation_site DROP COLUMN receiver_type_id;
ALTER TABLE relation_site DROP COLUMN certainty;
INSERT INTO relation VALUES(1,2,1,0,'none',0,0,0,1);
INSERT INTO relation_site VALUES(1,2,1,0,1,7,9,42);
PRAGMA user_version=7;
)sql");
  sqlite3_close(database);
  return created;
}

} // namespace

bool verifyVersionSevenMigration(const std::filesystem::path &path) {
  std::int64_t tableCount = 0;
  if (!require(createVersionSeven(path, tableCount),
               "failed to create version-seven database"))
    return false;
  {
    facts::Storage migrated{path.string()};
  }
  {
    facts::Storage idempotent{path.string()};
  }
  sqlite3 *database = nullptr;
  if (!require(sqlite3_open(path.c_str(), &database) == SQLITE_OK,
               "failed to open migrated version-seven database"))
    return false;
  const auto valid =
      require(scalar(database, "PRAGMA user_version") == 10,
              "version-seven migration was not recorded") &&
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'relation_site')") == 10,
              "context columns were not added exactly once") &&
      require(scalar(database,
                     "SELECT COUNT(*) FROM relation_site WHERE "
                     "source_id=1 AND destination_id=2 AND offset=42") == 1,
              "migration did not preserve relation-site facts") &&
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'relation_site') WHERE pk>0") == 6,
              "migration changed the relation-site primary key") &&
      require(scalar(database, "SELECT COUNT(*) FROM pragma_foreign_key_list("
                               "'relation_site')") == 5,
              "migration changed relation-site foreign keys") &&
      require(scalar(database, "SELECT COUNT(*) FROM sqlite_master WHERE "
                               "type='table'") == tableCount,
              "migration introduced a table");
  sqlite3_close(database);
  return valid;
}

} // namespace storage_schema_test
