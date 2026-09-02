#include "tooling/StoredCompilationDatabase.h"
#include "storage/FileManager.h"
#include "tooling/CompilationCommandCodec.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <sqlite3.h>

#include <algorithm>
#include <cstdlib>
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
  if (sqlite3_exec(database, sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
    std::abort();
  }
}

void require(bool condition) {
  if (!condition) {
    std::abort();
  }
}

} // namespace

int main(int argc, char **argv) {
  require(argc == 2);
  const auto databasePath = std::filesystem::absolute(argv[1]);
  const auto root = databasePath.parent_path() / "stored-compilation-project";
  const auto source = root / "source.cpp";
  const auto secondSource = root / "second.cpp";
  const auto unrelatedSource = root / "unrelated.cpp";
  const auto include = root / "include";
  std::filesystem::remove(databasePath);
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(include);
  {
    std::ofstream output(source);
    output << "int stored_options_fixture;\n";
  }
  {
    std::ofstream output(secondSource);
    output << "int second_stored_options_fixture;\n";
  }
  {
    std::ofstream output(unrelatedSource);
    output << "int unrelated_stored_options_fixture;\n";
  }

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
  execute(
      database,
      "INSERT INTO directory(id,component_id,path) VALUES(11,7,'');"
      "INSERT INTO file(id,directory_id,name,driver,compile_options) "
      "VALUES(23,11,'source.cpp','/opt/rh/gcc-toolset-15/root/usr/bin/g++-15',"
      "'[\"-I<fixture>/include\",\"-o\",\"discard.o\","
      "\"-DVALUE=1\",\"--target=x86_64-redhat-linux\","
      "\"--gcc-toolchain=/opt/rh/gcc-toolset-15/root/usr\","
      "\"--sysroot=/target-sysroot\",\"-stdlib=libstdc++\","
      "\"-Werror\"]');"
      "INSERT INTO file(id,directory_id,name,driver,compile_options) "
      "VALUES(24,11,'second.cpp','/usr/bin/c++','[\"-DSECOND=1\"]');"
      "INSERT INTO file(id,directory_id,name,driver,compile_options) "
      "VALUES(25,11,'unrelated.cpp','/usr/bin/c++','[\"-DUNRELATED=1\"]')");
  sqlite3_close(database);

  const auto writeTime = std::filesystem::last_write_time(databasePath);
  const auto normalizedRoot = facts::normalizeCompilationPath(root);
  const auto normalizedSource = facts::normalizeCompilationPath(source);
  const auto normalizedInclude = facts::normalizeCompilationPath(include);
  const std::vector<std::string> requested{source.string()};
  auto stored =
      facts::loadStoredCompilationDatabase(databasePath.string(), requested);
  require(stored.has_value());
  require(std::filesystem::last_write_time(databasePath) == writeTime);
  require((*stored)->getAllCompileCommands().size() == 1);
  const auto commands = (*stored)->getCompileCommands(source.string());
  require(commands.size() == 1);
  require(commands.front().Directory == normalizedRoot.string());
  require(commands.front().Filename == normalizedSource.string());
  require(commands.front().Output.empty());
  const auto &arguments = commands.front().CommandLine;
  const std::vector<std::string> expectedArguments = {
      "/opt/rh/gcc-toolset-15/root/usr/bin/g++-15",
      "-I" + normalizedInclude.string(),
      "-DVALUE=1",
      "--target=x86_64-redhat-linux",
      "--gcc-toolchain=/opt/rh/gcc-toolset-15/root/usr",
      "--sysroot=/target-sysroot",
      "-stdlib=libstdc++",
      normalizedSource.string()};
  require(arguments == expectedArguments);
  require(arguments.front() == "/opt/rh/gcc-toolset-15/root/usr/bin/g++-15");
  require(contains(arguments, "-I" + normalizedInclude.string()));
  require(contains(arguments, "-DVALUE=1"));
  require(contains(arguments, "--target=x86_64-redhat-linux"));
  require(
      contains(arguments, "--gcc-toolchain=/opt/rh/gcc-toolset-15/root/usr"));
  require(contains(arguments, "--sysroot=/target-sysroot"));
  require(contains(arguments, "-stdlib=libstdc++"));
  require(contains(arguments, normalizedSource.string()));
  require(!contains(arguments, "-o"));
  require(!contains(arguments, "discard.o"));
  require(!contains(arguments, "-Werror"));
  require(
      (*stored)->getCompileCommands((root / "missing.cpp").string()).empty());

  const std::vector<std::string> multipleRequested{source.string(),
                                                   secondSource.string()};
  auto multiple = facts::loadStoredCompilationDatabase(databasePath.string(),
                                                       multipleRequested);
  require(multiple.has_value());
  require((*multiple)->getAllCompileCommands().size() == 2);
  require((*multiple)->getCompileCommands(source.string()).size() == 1);
  require((*multiple)->getCompileCommands(secondSource.string()).size() == 1);

  auto all = facts::loadStoredCompilationDatabase(databasePath.string());
  require(all.has_value());
  require((*all)->getAllCompileCommands().size() == 3);
  require((*all)->getCompileCommands(unrelatedSource.string()).size() == 1);

  require(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK);
  execute(database,
          "UPDATE file SET compile_options='{\"invalid\":true}' WHERE id=25");
  sqlite3_close(database);
  const auto malformedWriteTime =
      std::filesystem::last_write_time(databasePath);
  auto selectedWithMalformedUnrelated =
      facts::loadStoredCompilationDatabase(databasePath.string(), requested);
  require(selectedWithMalformedUnrelated.has_value());
  require(std::filesystem::last_write_time(databasePath) == malformedWriteTime);
  require((*selectedWithMalformedUnrelated)->getAllCompileCommands().size() ==
          1);
  all = facts::loadStoredCompilationDatabase(databasePath.string());
  require(!all);
  require(all.error() == "compile_options is not a JSON array");

  const std::vector<std::string> missingRequested{
      (root / "missing.cpp").string()};
  auto missing = facts::loadStoredCompilationDatabase(databasePath.string(),
                                                      missingRequested);
  require(!missing);
  require(missing.error() ==
          "requested source is not imported: '" +
              facts::logicalCompilationPath(root / "missing.cpp").string() +
              "'");

  require(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK);
  execute(database, "UPDATE file SET compile_options=NULL WHERE id=24");
  sqlite3_close(database);
  const std::vector<std::string> noCommandRequested{secondSource.string()};
  auto noCommand = facts::loadStoredCompilationDatabase(databasePath.string(),
                                                        noCommandRequested);
  require(!noCommand);
  require(noCommand.error() ==
          "no stored compile command for requested source '" +
              facts::logicalCompilationPath(secondSource).string() + "'");

  require(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK);
  execute(database,
          "INSERT INTO component(id,name,path,kind,semantic_universe_id) "
          "SELECT 8,'duplicate',path,'repo',1 FROM component WHERE id=7;"
          "INSERT INTO directory(id,component_id,path) VALUES(12,8,'');"
          "INSERT INTO file(id,directory_id,name,driver,compile_options) "
          "VALUES(26,12,'source.cpp','/usr/bin/c++','[\"-DDUPLICATE=1\"]')");
  sqlite3_close(database);
  auto ambiguous =
      facts::loadStoredCompilationDatabase(databasePath.string(), requested);
  require(!ambiguous);
  require(ambiguous.error() ==
          "ambiguous stored compile commands for requested source '" +
              facts::logicalCompilationPath(source).string() + "'");

  require(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK);
  execute(database,
          "DELETE FROM file WHERE id=26;"
          "DELETE FROM directory WHERE id=12;"
          "DELETE FROM component WHERE id=8;"
          "UPDATE file SET compile_options='{\"invalid\":true}' WHERE id=23");
  sqlite3_close(database);
  auto malformed =
      facts::loadStoredCompilationDatabase(databasePath.string(), requested);
  require(!malformed);
  require(malformed.error() == "compile_options is not a JSON array");
}
