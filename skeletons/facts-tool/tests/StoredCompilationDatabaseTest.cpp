#include "tooling/StoredCompilationDatabase.h"
#include "storage/FileManager.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <sqlite3.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

bool contains(const std::vector<std::string> &arguments,
              const std::string &value) {
  return std::ranges::find(arguments, value) != arguments.end();
}

void execute(sqlite3 *database, const char *sql) {
  assert(sqlite3_exec(database, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
}

} // namespace

int main(int argc, char **argv) {
  assert(argc == 2);
  const auto databasePath = std::filesystem::absolute(argv[1]);
  const auto root = databasePath.parent_path() / "stored-compilation-project";
  const auto source = root / "source.cpp";
  const auto include = root / "include";
  std::filesystem::remove(databasePath);
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(include);
  {
    std::ofstream output(source);
    output << "int stored_options_fixture;\n";
  }

  {
    facts::FileManager initialize(databasePath.string());
  }

  sqlite3 *database = nullptr;
  assert(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK);
  execute(database, "DELETE FROM component");
  sqlite3_stmt *component = nullptr;
  assert(sqlite3_prepare_v2(
             database,
             "INSERT INTO component(id,name,path,kind,semantic_universe_id) "
             "VALUES(7,'fixture',?1,'repo',1)",
             -1, &component, nullptr) == SQLITE_OK);
  const auto rootString = root.string();
  assert(sqlite3_bind_text(component, 1, rootString.c_str(), -1,
                           SQLITE_TRANSIENT) == SQLITE_OK);
  assert(sqlite3_step(component) == SQLITE_DONE);
  sqlite3_finalize(component);
  execute(
      database,
      "INSERT INTO directory(id,component_id,path) VALUES(11,7,'');"
      "INSERT INTO file(id,directory_id,name,driver,compile_options) "
      "VALUES(23,11,'source.cpp','/opt/rh/gcc-toolset-15/root/usr/bin/g++-15',"
      "'[\"-I<fixture>/include\",\"-o\",\"discard.o\","
      "\"-DVALUE=1\",\"--target=x86_64-redhat-linux\","
      "\"--gcc-toolchain=/opt/rh/gcc-toolset-15/root/usr\","
      "\"--sysroot=/target-sysroot\",\"-stdlib=libstdc++\","
      "\"-Werror\"]')");
  sqlite3_close(database);

  auto stored = facts::loadStoredCompilationDatabase(databasePath.string());
  assert(stored);
  const auto commands = (*stored)->getCompileCommands(source.string());
  assert(commands.size() == 1);
  const auto &arguments = commands.front().CommandLine;
  assert(arguments.front() == "/opt/rh/gcc-toolset-15/root/usr/bin/g++-15");
  assert(contains(arguments, "-I" + include.string()));
  assert(contains(arguments, "-DVALUE=1"));
  assert(contains(arguments, "--target=x86_64-redhat-linux"));
  assert(
      contains(arguments, "--gcc-toolchain=/opt/rh/gcc-toolset-15/root/usr"));
  assert(contains(arguments, "--sysroot=/target-sysroot"));
  assert(contains(arguments, "-stdlib=libstdc++"));
  assert(contains(arguments, source.string()));
  assert(!contains(arguments, "-o"));
  assert(!contains(arguments, "discard.o"));
  assert(!contains(arguments, "-Werror"));
  assert(
      (*stored)->getCompileCommands((root / "missing.cpp").string()).empty());

  assert(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK);
  execute(database,
          "UPDATE file SET compile_options='{\"invalid\":true}' WHERE id=23");
  sqlite3_close(database);
  auto malformed = facts::loadStoredCompilationDatabase(databasePath.string());
  assert(!malformed);
}
