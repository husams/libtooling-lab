#include "storage/FileManager.h"
#include "tooling/CompilationCommandCodec.h"
#include "tooling/StoredCompilationDatabase.h"

#include <sqlite3.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace {
void require(bool condition) {
  if (!condition)
    std::abort();
}

void execute(sqlite3 *database, const char *sql) {
  require(sqlite3_exec(database, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
}
} // namespace

int main(int argc, char **argv) {
  require(argc == 2);
  const auto databasePath = std::filesystem::absolute(argv[1]);
  const auto root = databasePath.parent_path() / "missing-command-project";
  const auto source = root / "source.cpp";
  std::filesystem::remove(databasePath);
  {
    facts::FileManager initialize(databasePath.string());
  }

  sqlite3 *database = nullptr;
  require(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK);
  execute(database, "DELETE FROM component");
  sqlite3_stmt *component = nullptr;
  require(sqlite3_prepare_v2(
              database,
              "INSERT INTO component(id,name,path,kind,semantic_universe_id) "
              "VALUES(7,'fixture',?1,'repo',1)",
              -1, &component, nullptr) == SQLITE_OK);
  const auto rootString = root.string();
  require(sqlite3_bind_text(component, 1, rootString.c_str(), -1,
                            SQLITE_TRANSIENT) == SQLITE_OK);
  require(sqlite3_step(component) == SQLITE_DONE);
  sqlite3_finalize(component);
  execute(database,
          "INSERT INTO directory(id,component_id,path) VALUES(11,7,'');"
          "INSERT INTO file(id,directory_id,name,driver,compile_options) "
          "VALUES(23,11,'source.cpp',NULL,NULL)");
  sqlite3_close(database);

  const std::vector<std::string> requested{source.string()};
  auto loaded =
      facts::loadStoredCompilationDatabase(databasePath.string(), requested);
  require(!loaded);
  require(loaded.error() == "no stored compile command for requested source '" +
                                facts::logicalCompilationPath(source).string() +
                                "'");
}
