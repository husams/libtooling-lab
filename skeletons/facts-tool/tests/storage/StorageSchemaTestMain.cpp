#include "StorageSchemaTestCases.h"
#include "StorageSchemaTestSupport.h"

#include <filesystem>
#include <string>

int main(int argc, char **argv) {
  if (!storage_schema_test::require(
          argc == 3,
          "usage: storage-schema-test FRESH_DATABASE LEGACY_DATABASE")) {
    return 1;
  }

  const std::filesystem::path fresh{argv[1]};
  const std::string legacy{argv[2]};
  return storage_schema_test::verifyReturnTypeStorage(legacy + ".return-type") &&
                 storage_schema_test::verifyFreshSchema(fresh) &&
                 storage_schema_test::verifyMigration(legacy) &&
                 storage_schema_test::verifyVersionOneMigration(legacy +
                                                                ".v1") &&
                 storage_schema_test::verifyVersionTwoMigration(legacy +
                                                                ".v2") &&
                 storage_schema_test::verifyVersionFiveMigration(legacy +
                                                                 ".v5") &&
                 storage_schema_test::verifyVersionSevenMigration(legacy +
                                                                  ".v7") &&
                 storage_schema_test::verifyFileSchemaRollback(legacy +
                                                               ".file-rollback")
             ? 0
             : 1;
}
