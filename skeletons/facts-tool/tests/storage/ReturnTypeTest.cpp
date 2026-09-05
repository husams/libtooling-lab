#include "StorageSchemaTestCases.h"
#include "StorageSchemaTestSupport.h"

#include "storage/SchemaMigration.h"
#include "storage/Storage.h"

#include <array>

namespace storage_schema_test {
namespace {

bool populateReturnTypes(const std::filesystem::path &path,
                         facts::SymbolId &callable) {
  facts::Storage storage{path.string()};
  facts::Function function{};
  function.id.file = 1;
  function.usr = "c:@F@return_type_test";
  function.qualifiedName = "return_type_test";
  auto saved = storage.save(function);
  if (!require(saved.has_value(), "cannot save return-type test function"))
    return false;
  callable = *saved;
  const facts::ReturnType initial{{0, 7}, "const int *", "int"};
  const facts::ReturnType replacement{{0, 8}, "long &", "long"};
  const facts::ReturnType missing{{99, 1}, "Missing", {}};
  const facts::ReturnType rolledBack{{0, 9}, "bool", "bool"};
  return require(storage.saveReturnType(callable, initial).has_value(),
                 "cannot persist predefined return type") &&
         require(storage.saveReturnType(callable, initial).has_value(),
                 "repeated return type failed") &&
         require(storage.saveReturnType(callable, replacement).has_value(),
                 "cannot replace return target") &&
         require(!storage.saveReturnType(callable, missing).has_value(),
                 "missing return target must fail foreign-key validation") &&
         require(storage.begin().has_value(), "cannot begin extraction") &&
         require(storage.saveReturnType(callable, rolledBack).has_value(),
                 "cannot save return type inside extraction") &&
         require(storage.rollback().has_value(), "cannot roll back extraction");
}

bool verifyReturnRows(sqlite3 *database, facts::SymbolId callable) {
  const auto key = std::to_string(packed(callable));
  return require(scalar(database,
                        "SELECT COUNT(*) FROM relation WHERE kind=21") == 1,
                 "return replacement or rollback left duplicate edges") &&
         require(scalar(database, "SELECT destination_id FROM relation "
                                  "WHERE source_id=" +
                                      key + " AND kind=21") == 8,
                 "failed return update lost the previous target") &&
         require(
             textScalar(database,
                        "SELECT canonical_type FROM callable_return_type "
                        "WHERE symbol_id=" +
                            key) == "long &",
             "return qualifiers did not survive persistence and rollback") &&
         require(scalar(database, "SELECT COUNT(*) FROM symbol WHERE id=9") ==
                     0,
                 "outer transaction rollback retained the builtin target") &&
         require(scalar(database,
                        "SELECT COUNT(*) FROM pragma_foreign_key_check") == 0,
                 "return types violate foreign keys");
}

bool migrateReturnTypes(sqlite3 *database, facts::SymbolId callable) {
  const auto key = std::to_string(packed(callable));
  if (!execute(database,
               "DROP TABLE callable_return_type; PRAGMA user_version=8;"))
    return false;
  // Exercise the migration itself, before Storage's fresh-schema initializer
  // can hide a missing upgrade step by creating the table.
  return require(facts::storage::migrateSchema(database).has_value(),
                 "version-eight migration failed") &&
         require(facts::storage::migrateSchema(database).has_value(),
                 "repeated version-eight migration failed") &&
         require(scalar(database, "PRAGMA user_version") == 9,
                 "return-type migration did not record version nine") &&
         require(
             scalar(database, "SELECT COUNT(*) FROM callable_return_type") == 0,
             "migration invented unavailable return spellings") &&
         require(scalar(database, "SELECT destination_id FROM relation "
                                  "WHERE source_id=" +
                                      key + " AND kind=21") == 8,
                 "migration lost the existing return relation") &&
         require(textScalar(database, "SELECT usr FROM symbol WHERE id=" +
                                          key) == "c:@F@return_type_test",
                 "migration changed callable identity");
}

} // namespace

bool verifyReturnTypeStorage(const std::filesystem::path &path) {
  removeDatabase(path);
  facts::SymbolId callable{};
  if (!populateReturnTypes(path, callable))
    return false;
  sqlite3 *database = nullptr;
  if (!require(sqlite3_open(path.c_str(), &database) == SQLITE_OK,
               "cannot inspect return-type database"))
    return false;
  const auto valid = verifyReturnRows(database, callable) &&
                     migrateReturnTypes(database, callable);
  sqlite3_close(database);
  if (!valid)
    return false;
  {
    facts::Storage storage{path.string()};
    if (!require(storage.saveReturnType(callable, {{0, 8}, "long &", "long"})
                     .has_value(),
                 "cannot write return facts after upgrade"))
      return false;
  }
  if (sqlite3_open(path.c_str(), &database) != SQLITE_OK)
    return false;
  const auto cascade =
      execute(database, "PRAGMA foreign_keys=ON; DELETE FROM symbol WHERE id=" +
                            std::to_string(packed(callable))) &&
      require(scalar(database, "SELECT COUNT(*) FROM callable_return_type") ==
                  0,
              "deleted callable retained return spelling") &&
      require(scalar(database, "SELECT COUNT(*) FROM relation WHERE kind=21") ==
                  0,
              "deleted callable retained return edge");
  sqlite3_close(database);
  return cascade;
}

} // namespace storage_schema_test
