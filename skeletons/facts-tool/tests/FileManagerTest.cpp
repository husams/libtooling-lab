#include "storage/FileManager.h"
#include "storage/FileIdentity.h"

#include <sqlite3.h>

#include <array>
#include <cassert>
#include <filesystem>
#include <future>
#include <string>
#include <vector>

namespace {

int scalar(sqlite3 *database, const char *sql) {
  sqlite3_stmt *statement = nullptr;
  assert(sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) ==
         SQLITE_OK);
  assert(sqlite3_step(statement) == SQLITE_ROW);
  const int value = sqlite3_column_int(statement, 0);
  sqlite3_finalize(statement);
  return value;
}

} // namespace

int main(int argc, char **argv) {
  assert(argc == 4);
  const std::filesystem::path databasePath = argv[1];
  const std::filesystem::path source = std::filesystem::canonical(argv[2]);
  const std::filesystem::path header = std::filesystem::canonical(argv[3]);
  std::filesystem::remove(databasePath);

  const std::vector<std::string> paths = {source.string(), header.string()};

  const facts::ProjectClone clone{
      .path = source.parent_path().parent_path().string()};
  const std::array components{
      facts::ProjectComponent{.id = 1, .path = ".", .repositoryId = 1},
      facts::ProjectComponent{.id = 2, .path = "fixtures", .repositoryId = 1},
  };
  const auto nested = facts::identifyFile(components, clone, source);
  assert(nested && nested->componentId == 2 && nested->directory.empty() &&
         nested->name == source.filename());

  const std::array versioned{
      facts::ProjectComponent{
          .id = 3, .path = ".", .version = "fixtures", .repositoryId = 1},
  };
  const auto versionedSource = facts::identifyFile(versioned, clone, source);
  assert(versionedSource && versionedSource->componentId == 3 &&
         versionedSource->directory.empty());

  const std::array ungrouped{
      facts::ProjectComponent{.id = 4, .path = source.parent_path().string()}};
  const auto ungroupedHeader =
      facts::identifyFile(ungrouped, facts::ProjectClone{}, header);
  assert(ungroupedHeader && ungroupedHeader->componentId == 4 &&
         ungroupedHeader->name == header.filename());

  const auto root = facts::identifyFile(ungrouped, facts::ProjectClone{},
                                        source.parent_path());
  assert(!root && root.error() == std::make_error_code(
                                      std::errc::no_such_file_or_directory));
  const auto outside =
      facts::identifyFile(components, clone, databasePath.parent_path());
  assert(!outside &&
         outside.error() ==
             std::make_error_code(std::errc::no_such_file_or_directory));

  facts::FileManager first(databasePath.string());
  facts::FileManager second(databasePath.string());
  auto firstImport =
      std::async(std::launch::async, [&] { return first.addBulk(paths); });
  auto secondImport =
      std::async(std::launch::async, [&] { return second.addBulk(paths); });
  assert(firstImport.get());
  assert(secondImport.get());

  const auto firstHeader = first.getId(header.string());
  const auto secondHeader = second.getId(header.string());
  assert(firstHeader && secondHeader && *firstHeader == *secondHeader);
  assert(*firstHeader >= facts::firstPhysicalFileId);

  const auto unseen =
      first.getId(std::filesystem::canonical(__FILE__).string());
  assert(!unseen);

  sqlite3 *database = nullptr;
  assert(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK);
  assert(scalar(database, "SELECT COUNT(*) FROM file WHERE id=0") == 0);
  assert(scalar(database, "SELECT COUNT(*) FROM file") == 2);
  assert(scalar(database,
                "SELECT COUNT(*) FROM directory WHERE path GLOB '/*'") == 0);
  assert(scalar(database, "SELECT COUNT(*) FROM component WHERE path='/'") ==
         1);
  assert(scalar(database, "SELECT COUNT(*) FROM file WHERE name GLOB '*/*'") ==
         0);
  assert(scalar(database, "SELECT COUNT(*) FROM pragma_table_info('file') "
                          "WHERE name IN ('directory_id', 'name')") == 2);
  sqlite3_close(database);

  const auto sourceOnly =
      facts::FileManager::openReadOnly(databasePath.string());
  assert(!sourceOnly);
  assert(sourceOnly.error().contains("project configuration is incomplete"));
  assert(sourceOnly.error().contains("facts-tool import"));

  const auto additivePath = databasePath.string() + ".working-directory";
  std::filesystem::remove(additivePath);
  {
    facts::FileManager initialize(additivePath);
  }
  assert(sqlite3_open(additivePath.c_str(), &database) == SQLITE_OK);
  assert(sqlite3_exec(database,
                      "ALTER TABLE file DROP COLUMN working_directory", nullptr,
                      nullptr, nullptr) == SQLITE_OK);
  sqlite3_close(database);
  {
    facts::FileManager migrated(additivePath);
  }
  assert(sqlite3_open(additivePath.c_str(), &database) == SQLITE_OK);
  assert(scalar(database, "SELECT COUNT(*) FROM pragma_table_info('file') "
                          "WHERE name='working_directory'") == 1);
  sqlite3_close(database);

  const auto legacyPath = databasePath.string() + ".legacy";
  std::filesystem::remove(legacyPath);
  sqlite3 *legacyDatabase = nullptr;
  assert(sqlite3_open(legacyPath.c_str(), &legacyDatabase) == SQLITE_OK);
  assert(sqlite3_exec(legacyDatabase,
                      "CREATE TABLE file(id INTEGER PRIMARY KEY, "
                      "path TEXT NOT NULL UNIQUE)",
                      nullptr, nullptr, nullptr) == SQLITE_OK);
  sqlite3_stmt *legacyInsert = nullptr;
  assert(sqlite3_prepare_v2(legacyDatabase,
                            "INSERT INTO file(id, path) VALUES(17, ?1)", -1,
                            &legacyInsert, nullptr) == SQLITE_OK);
  assert(sqlite3_bind_text(legacyInsert, 1, source.c_str(), -1,
                           SQLITE_TRANSIENT) == SQLITE_OK);
  assert(sqlite3_step(legacyInsert) == SQLITE_DONE);
  sqlite3_finalize(legacyInsert);
  sqlite3_close(legacyDatabase);

  facts::FileManager migrated(legacyPath);
  const auto migratedSource = migrated.getId(source.string());
  assert(migratedSource && *migratedSource == 17);
  assert(migrated.addBulk(paths));
  const auto migratedHeader = migrated.getId(header.string());
  assert(migratedHeader && *migratedHeader >= facts::firstPhysicalFileId);

  assert(sqlite3_open(legacyPath.c_str(), &legacyDatabase) == SQLITE_OK);
  assert(scalar(legacyDatabase,
                "SELECT COUNT(*) FROM pragma_table_info('file') "
                "WHERE name='path'") == 0);
  assert(scalar(legacyDatabase, "SELECT COUNT(*) FROM file") == 2);
  sqlite3_close(legacyDatabase);

  const auto sharedPath = databasePath.string() + ".cpp-indexer";
  std::filesystem::remove(sharedPath);
  {
    facts::FileManager initializeSharedSchema(sharedPath);
  }
  sqlite3 *sharedDatabase = nullptr;
  assert(sqlite3_open(sharedPath.c_str(), &sharedDatabase) == SQLITE_OK);
  assert(sqlite3_exec(sharedDatabase,
                      "DELETE FROM component;"
                      "INSERT INTO repository(id,name,kind,active_clone_id) "
                      "VALUES(1,'shared','repo',1);"
                      "INSERT INTO clone(id,repository_id,path,label) "
                      "VALUES(1,1,'/tmp','active');",
                      nullptr, nullptr, nullptr) == SQLITE_OK);

  sqlite3_stmt *sharedRoot = nullptr;
  assert(sqlite3_prepare_v2(sharedDatabase,
                            "UPDATE clone SET path=?1 WHERE id=1", -1,
                            &sharedRoot, nullptr) == SQLITE_OK);
  const auto cloneRoot =
      source.parent_path().parent_path().parent_path().string();
  assert(sqlite3_bind_text(sharedRoot, 1, cloneRoot.c_str(), -1,
                           SQLITE_TRANSIENT) == SQLITE_OK);
  assert(sqlite3_step(sharedRoot) == SQLITE_DONE);
  sqlite3_finalize(sharedRoot);
  assert(sqlite3_exec(
             sharedDatabase,
             "INSERT INTO component(id,name,path,kind,repository_id,"
             "semantic_universe_id) VALUES(7,'facts-tool','tests','repo',1,1);"
             "INSERT INTO directory(id,component_id,path) "
             "VALUES(11,7,'fixtures')",
             nullptr, nullptr, nullptr) == SQLITE_OK);
  sqlite3_stmt *sharedFile = nullptr;
  assert(sqlite3_prepare_v2(sharedDatabase,
                            "INSERT INTO file(id,directory_id,name,indexed) "
                            "VALUES(23,11,?1,1)",
                            -1, &sharedFile, nullptr) == SQLITE_OK);
  const auto headerName = header.filename().string();
  assert(sqlite3_bind_text(sharedFile, 1, headerName.c_str(), -1,
                           SQLITE_TRANSIENT) == SQLITE_OK);
  assert(sqlite3_step(sharedFile) == SQLITE_DONE);
  sqlite3_finalize(sharedFile);
  sqlite3_close(sharedDatabase);

  facts::FileManager shared(sharedPath);
  auto sharedPaths = paths;
  const auto external =
      std::filesystem::canonical(std::filesystem::path(cloneRoot) /
                                 "src/storage/FileManager.cpp")
          .string();
  sharedPaths.push_back(external);
  assert(shared.addBulk(sharedPaths));
  const auto relativeHeader =
      std::filesystem::relative(header, std::filesystem::current_path())
          .string();
  const auto sharedHeader = shared.getId(header.string());
  const auto sharedRelativeHeader = shared.getId(relativeHeader);
  const auto sharedSource = shared.getId(source.string());
  const auto sharedExternal = shared.getId(external);
  assert(sharedHeader && *sharedHeader == 23);
  assert(sharedRelativeHeader && *sharedRelativeHeader == 23);
  assert(sharedSource && *sharedSource >= facts::firstPhysicalFileId);
  assert(!sharedExternal);

  assert(sqlite3_open(sharedPath.c_str(), &sharedDatabase) == SQLITE_OK);
  assert(scalar(sharedDatabase, "SELECT COUNT(*) FROM component") == 1);
  assert(scalar(sharedDatabase,
                "SELECT COUNT(*) FROM pragma_table_info('file') WHERE name IN "
                "('mtime','md5','compile_options','driver','indexed',"
                "'indexed_at','args_overridden')") == 7);
  assert(scalar(sharedDatabase,
                "SELECT COUNT(*) FROM directory WHERE path='fixtures'") == 1);
  assert(scalar(sharedDatabase, "SELECT COUNT(*) FROM file") == 2);
  sqlite3_close(sharedDatabase);
}
