#include "StorageSchemaTestCases.h"
#include "StorageSchemaTestSupport.h"

#include "storage/FileDatabase.h"
#include "storage/Storage.h"

#include <sqlite3.h>

#include <filesystem>
#include <stdexcept>

namespace storage_schema_test {

bool createLegacyDatabase(const std::filesystem::path &path) {
  removeDatabase(path);
  sqlite3 *database = nullptr;
  if (sqlite3_open(path.c_str(), &database) != SQLITE_OK) {
    return false;
  }
  const auto created = execute(database, R"sql(
CREATE TABLE symbol (
  id INTEGER PRIMARY KEY, file_id INTEGER NOT NULL, file_index INTEGER NOT NULL,
  identity TEXT NOT NULL, node INTEGER NOT NULL, kind INTEGER NOT NULL,
  sub_kind INTEGER NOT NULL, lang INTEGER NOT NULL, properties INTEGER NOT NULL,
  usr TEXT NOT NULL, qualified_name TEXT NOT NULL, line INTEGER NOT NULL,
  col INTEGER NOT NULL, offset INTEGER NOT NULL, flags INTEGER NOT NULL);
CREATE TABLE parameter (
  symbol_id INTEGER NOT NULL, position INTEGER NOT NULL, name TEXT NOT NULL,
  type INTEGER NOT NULL, line INTEGER NOT NULL, col INTEGER NOT NULL,
  offset INTEGER NOT NULL, region_offset INTEGER NOT NULL,
  region_size INTEGER NOT NULL, flags INTEGER NOT NULL,
  has_default INTEGER NOT NULL, PRIMARY KEY(symbol_id,position)) WITHOUT ROWID;
CREATE TABLE relation (
  source_id INTEGER NOT NULL, destination_id INTEGER NOT NULL,
  kind INTEGER NOT NULL, position INTEGER NOT NULL, flags INTEGER NOT NULL,
  count INTEGER NOT NULL,
  PRIMARY KEY(source_id,destination_id,kind,position)) WITHOUT ROWID;
CREATE TABLE template_argument (
  symbol_id INTEGER NOT NULL, position INTEGER NOT NULL, name TEXT NOT NULL,
  type_id INTEGER NOT NULL, flags INTEGER NOT NULL,
  PRIMARY KEY(symbol_id,position)) WITHOUT ROWID;
CREATE TABLE template_parameter (
  symbol_id INTEGER NOT NULL, position INTEGER NOT NULL, value TEXT NOT NULL,
  type_id INTEGER NOT NULL, flags INTEGER NOT NULL, kind INTEGER NOT NULL,
  pack_index INTEGER NOT NULL, PRIMARY KEY(symbol_id,position)) WITHOUT ROWID;
INSERT INTO symbol VALUES(1,1,0,'legacy',1,0,0,0,0,'legacy','legacy',1,1,0,27264070);
INSERT INTO parameter VALUES(1,0,'p',1,1,1,0,0,1,63,0);
INSERT INTO relation VALUES(1,1,2,0,29,1);
INSERT INTO template_argument VALUES(1,0,'T',0,7);
INSERT INTO template_parameter VALUES(1,0,'',0,63,1,-1);
)sql");
  sqlite3_close(database);
  return created;
}

bool createVersionOneDatabase(const std::filesystem::path &path) {
  removeDatabase(path);
  {
    facts::Storage current{path.string()};
  }

  sqlite3 *database = nullptr;
  if (sqlite3_open(path.c_str(), &database) != SQLITE_OK) {
    return false;
  }
  const auto created = execute(database, R"sql(
DROP INDEX IF EXISTS idx_parameter_default_evaluated;
DROP TABLE parameter_default;
DROP INDEX IF EXISTS idx_variable_initializer_evaluated;
DROP TABLE variable_initializer;
ALTER TABLE symbol DROP COLUMN has_extern_storage;
PRAGMA user_version=1;
)sql");
  sqlite3_close(database);
  return created;
}

bool verifyVersionOneMigration(const std::filesystem::path &path) {
  if (!require(createVersionOneDatabase(path),
               "failed to create version-one database")) {
    return false;
  }
  {
    facts::Storage migrated{path.string()};
  }

  sqlite3 *database = nullptr;
  if (!require(sqlite3_open(path.c_str(), &database) == SQLITE_OK,
               "failed to open version-one database")) {
    return false;
  }
  const auto valid =
      require(scalar(database,
                     "SELECT COUNT(*) FROM pragma_table_info('symbol') "
                     "WHERE name='has_extern_storage'") == 1,
              "extern storage column was not migrated") &&
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'variable_initializer')") == 4,
              "initializer table was not migrated from version one") &&
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'parameter_default')") == 5,
              "parameter default table was not migrated from version one") &&
      require(scalar(database, "PRAGMA user_version") == 7,
              "version-one migration was not recorded");
  sqlite3_close(database);
  return valid;
}

bool createVersionTwoDatabase(const std::filesystem::path &path) {
  removeDatabase(path);
  {
    facts::Storage current{path.string()};
  }

  sqlite3 *database = nullptr;
  if (sqlite3_open(path.c_str(), &database) != SQLITE_OK) {
    return false;
  }
  const auto created = execute(database, R"sql(
DROP INDEX IF EXISTS idx_parameter_default_evaluated;
DROP TABLE parameter_default;
PRAGMA user_version=2;
)sql");
  sqlite3_close(database);
  return created;
}

bool verifyVersionTwoMigration(const std::filesystem::path &path) {
  if (!require(createVersionTwoDatabase(path),
               "failed to create version-two database")) {
    return false;
  }
  {
    facts::Storage migrated{path.string()};
  }

  sqlite3 *database = nullptr;
  if (!require(sqlite3_open(path.c_str(), &database) == SQLITE_OK,
               "failed to open version-two database")) {
    return false;
  }
  const auto valid =
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'parameter_default')") == 5,
              "parameter default table was not migrated from version two") &&
      require(scalar(database, "PRAGMA user_version") == 7,
              "version-two migration was not recorded");
  sqlite3_close(database);
  return valid;
}

bool verifyVersionFiveMigration(const std::filesystem::path &path) {
  removeDatabase(path);
  {
    facts::Storage current{path.string()};
  }

  sqlite3 *database = nullptr;
  if (!require(sqlite3_open(path.c_str(), &database) == SQLITE_OK,
               "failed to open version-five database")) {
    return false;
  }
  const auto prepared = execute(database, R"sql(
DROP INDEX idx_symbol_unique_usr;
ALTER TABLE symbol ADD COLUMN identity TEXT NOT NULL DEFAULT '';
CREATE UNIQUE INDEX idx_symbol_file_identity
  ON symbol(((id >> 32) & 4294967295), identity);
CREATE UNIQUE INDEX idx_symbol_unique_usr ON symbol(usr) WHERE usr <> '';
PRAGMA user_version=5;
)sql");
  sqlite3_close(database);
  if (!require(prepared, "failed to create version-five database")) {
    return false;
  }

  {
    facts::Storage migrated{path.string()};
  }
  if (!require(sqlite3_open(path.c_str(), &database) == SQLITE_OK,
               "failed to open migrated version-five database")) {
    return false;
  }
  const auto valid =
      require(usrIsOnlySymbolIdentity(database),
              "version-five migration retained a separate symbol identity") &&
      require(scalar(database, "PRAGMA user_version") == 7,
              "version-five migration was not recorded");
  sqlite3_close(database);
  return valid;
}

bool verifyMigration(const std::filesystem::path &path) {
  if (!require(createLegacyDatabase(path),
               "failed to create legacy database")) {
    return false;
  }
  {
    facts::Storage migrated{path.string()};
  }

  sqlite3 *database = nullptr;
  if (!require(sqlite3_open(path.c_str(), &database) == SQLITE_OK,
               "failed to open migrated database")) {
    return false;
  }
  const auto valid =
      require(noPackedFlags(database), "migration retained packed flags") &&
      require(noRedundantSymbolIdColumns(database),
              "migration retained redundant symbol id columns") &&
      require(usrIsOnlySymbolIdentity(database),
              "migration retained a separate symbol identity") &&
      require(scalar(database,
                     "SELECT COUNT(*) FROM symbol WHERE access='private' AND "
                     "is_definition=1 AND is_const=1 AND "
                     "ref_qualifier='rvalue' AND "
                     "has_extern_storage=1 AND "
                     "constant_evaluation='consteval' AND is_noexcept=1") == 1,
              "migrated symbol properties are incorrect") &&
      require(
          scalar(database,
                 "SELECT COUNT(*) FROM parameter WHERE is_pointer=1 AND "
                 "is_lvalue_reference=1 AND is_rvalue_reference=1 AND "
                 "is_forwarding_reference=1 AND is_const=1 AND is_pack=1") == 1,
          "migrated parameter properties are incorrect") &&
      require(
          scalar(database,
                 "SELECT COUNT(*) FROM relation WHERE access='protected' "
                 "AND is_virtual_base=1 AND is_implicit=1 AND is_lexical=1") ==
              1,
          "migrated relation properties are incorrect") &&
      require(scalar(database, "SELECT COUNT(*) FROM template_argument WHERE "
                               "is_parameter_pack=1 AND is_non_type=1 AND "
                               "is_template_template=1") == 1,
              "migrated template argument properties are incorrect") &&
      require(scalar(database,
                     "SELECT COUNT(*) FROM template_parameter WHERE "
                     "is_pointer=1 AND is_lvalue_reference=1 AND "
                     "is_rvalue_reference=1 AND is_forwarding_reference=1 "
                     "AND is_const=1 AND is_pack=1") == 1,
              "migrated template parameter properties are incorrect") &&
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'variable_initializer')") == 4,
              "initializer schema was not migrated") &&
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'parameter_default')") == 5,
              "parameter default schema was not migrated") &&
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'enumeration')") == 4,
              "enumeration schema was not migrated") &&
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'enumerator')") == 3,
              "enumerator schema was not migrated") &&
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'relation_site')") == 8,
              "relation-site schema was not migrated") &&
      require(scalar(database, "PRAGMA user_version") == 7,
              "migration version was not recorded");
  sqlite3_close(database);
  return valid;
}

bool verifyFileSchemaRollback(const std::filesystem::path &path) {
  removeDatabase(path);
  sqlite3 *database = nullptr;
  if (!require(sqlite3_open(path.c_str(), &database) == SQLITE_OK,
               "failed to create rollback file database")) {
    return false;
  }
  const auto created = execute(database, R"sql(
CREATE TABLE file(id INTEGER PRIMARY KEY, path TEXT NOT NULL UNIQUE);
INSERT INTO file(id,path) VALUES(0,'/invalid.cpp');
)sql");
  sqlite3_close(database);
  if (!require(created, "failed to prepare rollback file database")) {
    return false;
  }

  bool failed = false;
  try {
    facts::FileDatabase migrated{path.string()};
  } catch (const std::runtime_error &) {
    failed = true;
  }
  if (!require(failed, "invalid legacy file schema unexpectedly migrated")) {
    return false;
  }

  if (!require(sqlite3_open(path.c_str(), &database) == SQLITE_OK,
               "failed to inspect rolled-back file database")) {
    return false;
  }
  const auto rolledBack =
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info('file') "
                               "WHERE name='path'") == 1,
              "rollback did not restore the legacy file table") &&
      require(scalar(database,
                     "SELECT COUNT(*) FROM sqlite_master "
                     "WHERE type='table' AND name='legacy_file'") == 0,
              "rollback retained the renamed legacy file table") &&
      require(scalar(database, "SELECT COUNT(*) FROM file WHERE id=0") == 1,
              "rollback did not restore the legacy file row");
  sqlite3_close(database);
  return rolledBack;
}

} // namespace storage_schema_test
