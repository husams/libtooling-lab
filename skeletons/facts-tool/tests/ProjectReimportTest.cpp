#include "storage/FileDatabase.h"

#include <sqlite3.h>

#include <cassert>
#include <filesystem>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using Rows = std::vector<std::vector<std::string>>;

Rows query(sqlite3 *database, const char *sql) {
  sqlite3_stmt *statement = nullptr;
  assert(sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) ==
         SQLITE_OK);
  Rows rows;
  int result;
  while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
    std::vector<std::string> row;
    for (int column = 0; column < sqlite3_column_count(statement); ++column) {
      const auto *value = sqlite3_column_text(statement, column);
      row.emplace_back(value ? reinterpret_cast<const char *>(value) : "");
    }
    rows.push_back(std::move(row));
  }
  assert(result == SQLITE_DONE);
  sqlite3_finalize(statement);
  return rows;
}

void assertSingleProject(sqlite3 *database, const std::string &fileCount) {
  assert((query(database,
                "SELECT (SELECT count(*) FROM repository),"
                "(SELECT count(*) FROM clone),"
                "(SELECT count(*) FROM component "
                "WHERE repository_id IS NOT NULL),"
                "(SELECT count(*) FROM directory),"
                "(SELECT count(*) FROM file)") ==
          Rows{{"1", "1", "1", fileCount, fileCount}}));
  assert((query(database,
                "SELECT count(*) FROM repository JOIN clone "
                "ON repository.active_clone_id=clone.id "
                "WHERE clone.repository_id=repository.id") == Rows{{"1"}}));
  assert(query(database, "PRAGMA foreign_key_check").empty());
}

Rows identities(sqlite3 *database) {
  return query(database,
               "SELECT repository.id,clone.id,component.id,directory.id,file.id "
               "FROM file JOIN directory ON directory.id=file.directory_id "
               "JOIN component ON component.id=directory.component_id "
               "JOIN repository ON repository.id=component.repository_id "
               "JOIN clone ON clone.id=repository.active_clone_id "
               "WHERE directory.path='src'");
}

} // namespace

int main(int argc, char **argv) {
  assert(argc == 2);
  const auto databasePath = std::filesystem::absolute(argv[1]);
  std::filesystem::remove(databasePath);
  const auto checkout = databasePath.parent_path() / "reimport-checkout";
  facts::ProjectConfiguration configuration{
      .repositoryName = "checkout-label",
      .remoteUrl = "",
      .activeClone = {.path = checkout.string(), .label = "checkout-label"},
      .components = {{.name = "checkout-label",
                      .path = ".",
                      .version = std::nullopt,
                      .repositoryId = std::nullopt}},
      .files = {{.componentPath = ".",
                 .directory = "src",
                 .name = "main.cpp",
                 .driver = "clang++",
                 .workingDirectory = checkout.string(),
                 .compileOptions = "[\"-DVALUE=1\"]"}}};
  // Each import uses a fresh connection, just as separate CLI invocations do.
  const auto reimport = [&] {
    facts::FileDatabase files(databasePath.string());
    assert(files.replaceProjectConfiguration(configuration));
  };
  reimport();

  sqlite3 *database = nullptr;
  assert(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK);
  assertSingleProject(database, "1");
  const auto originalIds = identities(database);
  assert(originalIds.size() == 1);
  const auto fileId =
      static_cast<facts::FileId>(std::stoul(originalIds.front().back()));
  {
    facts::FileDatabase files(databasePath.string());
    const facts::FileIndexRecord indexed{
        .id = fileId,
        .indexedAt = "2026-09-16T10:00:00Z",
        .mtime = 12345.5,
        .factsDb = "/tmp/reimport-facts.sqlite",
        .gitCommit = std::string(40, 'a')};
    assert(files.markIndexed(std::span(&indexed, 1)));
  }
  const auto indexState = query(
      database, "SELECT indexed,indexed_at,mtime,facts_db,git_commit FROM file");

  for (int attempt = 0; attempt < 2; ++attempt) {
    reimport();
    assertSingleProject(database, "1");
    assert(identities(database) == originalIds);
    assert(query(database, "SELECT indexed,indexed_at,mtime,facts_db,git_commit "
                           "FROM file") == indexState);
  }

  // Discovering the origin name must update the checkout's existing identity.
  configuration.repositoryName = "origin-name";
  configuration.remoteUrl = "https://example.invalid/team/origin-name.git";
  configuration.activeClone.label = "renamed-checkout";
  configuration.components.front().name = "origin-name";
  reimport();
  assertSingleProject(database, "1");
  assert(identities(database) == originalIds);
  assert((query(database, "SELECT name,remote_url FROM repository") ==
          Rows{{configuration.repositoryName, configuration.remoteUrl}}));
  assert((query(database, "SELECT label FROM clone") ==
          Rows{{configuration.activeClone.label}}));
  assert((query(database, "SELECT name FROM component "
                         "WHERE repository_id IS NOT NULL") ==
          Rows{{"origin-name"}}));
  assert(query(database, "SELECT indexed,indexed_at,mtime,facts_db,git_commit "
                         "FROM file") == indexState);

  auto &source = configuration.files.front();
  source.driver = "g++";
  source.workingDirectory = (checkout / "build").string();
  source.compileOptions = "[\"-DVALUE=2\"]";
  for (int attempt = 0; attempt < 2; ++attempt) {
    reimport();
    assertSingleProject(database, "1");
    assert(identities(database) == originalIds);
    assert((query(database,
                  "SELECT driver,working_directory,compile_options FROM file") ==
            Rows{{source.driver, source.workingDirectory,
                  source.compileOptions}}));
    assert((query(database, "SELECT indexed,indexed_at,mtime,facts_db,git_commit "
                           "FROM file") == Rows{{"0", "", "", "", ""}}));
  }

  // Equal basenames in different directories are distinct source identities.
  auto second = source;
  second.directory = "other";
  configuration.files.push_back(second);
  reimport();
  assertSingleProject(database, "2");
  assert(identities(database) == originalIds);
  assert((query(database, "SELECT count(DISTINCT id) FROM file "
                         "WHERE name='main.cpp'") == Rows{{"2"}}));
  const auto twoFiles = query(database, "SELECT * FROM file ORDER BY id");
  reimport();
  assertSingleProject(database, "2");
  assert(query(database, "SELECT * FROM file ORDER BY id") == twoFiles);

  // Importing a single source must preserve the other source's complete row,
  // including its stored command and existing extraction freshness state.
  {
    facts::FileDatabase files(databasePath.string());
    const facts::FileIndexRecord indexed{
        .id = fileId,
        .indexedAt = "2026-09-19T10:00:00Z",
        .mtime = 23456.5,
        .factsDb = "/tmp/incremental-facts.sqlite",
        .gitCommit = std::string(40, 'b')};
    assert(files.markIndexed(std::span(&indexed, 1)));
  }
  constexpr auto untouchedSource =
      "SELECT file.* FROM file JOIN directory ON directory.id=file.directory_id "
      "WHERE directory.path='src'";
  const auto beforePartialImport = query(database, untouchedSource);
  configuration.files = {second};
  configuration.files.front().compileOptions = "[\"-DVALUE=3\"]";
  reimport();
  assertSingleProject(database, "2");
  assert(identities(database) == originalIds);
  assert(query(database, untouchedSource) == beforePartialImport);
  assert((query(database,
                "SELECT file.compile_options FROM file JOIN directory "
                "ON directory.id=file.directory_id WHERE directory.path='other'") ==
          Rows{{configuration.files.front().compileOptions}}));
  const auto afterPartialImport = query(database, "SELECT * FROM file ORDER BY id");
  reimport();
  assertSingleProject(database, "2");
  assert(query(database, "SELECT * FROM file ORDER BY id") == afterPartialImport);

  // A name owned by another checkout must not silently merge repositories.
  assert(sqlite3_exec(
             database,
             "INSERT INTO repository(id,name,active_clone_id) "
             "VALUES(99,'occupied-name',99);"
             "INSERT INTO clone(id,repository_id,path,label) "
             "VALUES(99,99,'/unrelated-checkout','unrelated');"
             "INSERT INTO component(id,name,path,repository_id) "
             "VALUES(99,'unrelated','.',99);"
             "INSERT INTO directory(id,component_id,path) VALUES(99,99,'src');"
             "INSERT INTO file(id,directory_id,name,compile_options) "
             "VALUES(99,99,'main.cpp','[\"-DKEEP\"]');",
             nullptr, nullptr, nullptr) == SQLITE_OK);
  const auto snapshot = [&] {
    return std::vector<Rows>{
        query(database, "SELECT * FROM repository ORDER BY id"),
        query(database, "SELECT * FROM clone ORDER BY id"),
        query(database, "SELECT * FROM component ORDER BY id"),
        query(database, "SELECT * FROM directory ORDER BY id"),
        query(database, "SELECT * FROM file ORDER BY id"),
        query(database, "SELECT * FROM project_registry")};
  };
  const auto beforeConflict = snapshot();
  configuration.repositoryName = "occupied-name";
  {
    facts::FileDatabase files(databasePath.string());
    const auto rejected = files.replaceProjectConfiguration(configuration);
    assert(!rejected);
  }
  assert(snapshot() == beforeConflict);
  assert(query(database, "PRAGMA foreign_key_check").empty());
  sqlite3_close(database);
}
