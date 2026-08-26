#include "storage/FileManager.h"
#include "tooling/ProjectImport.h"
#include "tooling/StoredCompilationDatabase.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <sqlite3.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

namespace {

std::string scalarText(const std::filesystem::path &databasePath,
                       const char *sql) {
  sqlite3 *database = nullptr;
  assert(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK);
  sqlite3_stmt *statement = nullptr;
  assert(sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) ==
         SQLITE_OK);
  assert(sqlite3_step(statement) == SQLITE_ROW);
  const auto *value = sqlite3_column_text(statement, 0);
  const std::string result =
      value ? reinterpret_cast<const char *>(value) : std::string{};
  sqlite3_finalize(statement);
  sqlite3_close(database);
  return result;
}

class TestCompilationDatabase final
    : public clang::tooling::CompilationDatabase {
public:
  explicit TestCompilationDatabase(clang::tooling::CompileCommand command)
      : commands_{std::move(command)} {}

  std::vector<clang::tooling::CompileCommand>
  getCompileCommands(llvm::StringRef filePath) const override {
    return filePath == commands_.front().Filename
               ? commands_
               : std::vector<clang::tooling::CompileCommand>{};
  }

  std::vector<std::string> getAllFiles() const override {
    return {commands_.front().Filename};
  }

  std::vector<clang::tooling::CompileCommand>
  getAllCompileCommands() const override {
    return commands_;
  }

private:
  std::vector<clang::tooling::CompileCommand> commands_;
};

} // namespace

int main(int argc, char **argv) {
  assert(argc == 3);
  const auto databasePath = std::filesystem::absolute(argv[1]);
  const auto fixtureRoot = std::filesystem::canonical(argv[2]);
  const auto cloneOne = databasePath.parent_path() / "cpp-indexer";
  const auto cloneTwo = databasePath.parent_path() / "clone-two";
  const auto sourceRelative =
      std::filesystem::path("tests/fixtures/tu_one.cpp");
  std::filesystem::remove_all(cloneOne);
  std::filesystem::remove_all(cloneTwo);
  std::filesystem::create_directories(cloneOne / sourceRelative.parent_path());
  std::filesystem::create_directories(cloneTwo / sourceRelative.parent_path());
  std::filesystem::copy_file(fixtureRoot / sourceRelative,
                             cloneOne / sourceRelative,
                             std::filesystem::copy_options::overwrite_existing);
  std::filesystem::copy_file(fixtureRoot / sourceRelative,
                             cloneTwo / sourceRelative,
                             std::filesystem::copy_options::overwrite_existing);

  facts::FileManager files(databasePath.string());
  const auto source = cloneOne / sourceRelative;
  TestCompilationDatabase input(clang::tooling::CompileCommand{
      cloneOne.string(),
      source.string(),
      {"clang++", "-std=c++23", "-Itests/fixtures", source.string()},
      ""});
  const std::vector<std::string> fallbackSources{source.string()};
  auto imported = facts::importProjectConfiguration(
      files, input, fallbackSources,
      facts::ProjectImportOptions{.repositoryName = "facts-tool"});
  assert(imported && imported->importedFiles == 1);
  imported = facts::importProjectConfiguration(files, input, fallbackSources,
                                               facts::ProjectImportOptions{});
  assert(imported && imported->importedFiles == 1);
  assert(scalarText(databasePath, "SELECT name FROM repository") ==
         "cpp-indexer");
  assert(scalarText(databasePath, "SELECT path FROM clone") ==
         cloneOne.string());
  assert(scalarText(databasePath,
                    "SELECT name FROM component "
                    "WHERE repository_id IS NOT NULL") == "cpp-indexer");
  assert(scalarText(databasePath, "SELECT path FROM component "
                                  "WHERE repository_id IS NOT NULL") == ".");
  assert(scalarText(databasePath,
                    "SELECT directory.path FROM directory "
                    "JOIN component ON component.id=directory.component_id "
                    "WHERE component.repository_id IS NOT NULL") ==
         sourceRelative.parent_path().generic_string());
  assert(scalarText(databasePath,
                    "SELECT file.name FROM file "
                    "JOIN directory ON directory.id=file.directory_id "
                    "JOIN component ON component.id=directory.component_id "
                    "WHERE component.repository_id IS NOT NULL") ==
         sourceRelative.filename().generic_string());

  auto stored = facts::loadStoredCompilationDatabase(databasePath.string());
  assert(stored);
  auto commands = (*stored)->getCompileCommands(source.string());
  assert(commands.size() == 1);
  assert(commands.front().Directory == cloneOne.string());
  assert(commands.front().Filename == source.string());
  assert(std::ranges::find(commands.front().CommandLine,
                           "-I" + (cloneOne / "tests/fixtures").string()) !=
         commands.front().CommandLine.end());

  assert(files.addClone(
      "cpp-indexer",
      facts::ProjectClone{.path = cloneTwo.string(), .label = "second"}));
  assert(files.switchActiveClone("cpp-indexer", "second"));
  stored = facts::loadStoredCompilationDatabase(databasePath.string());
  assert(stored);
  commands = (*stored)->getAllCompileCommands();
  assert(commands.size() == 1);
  assert(commands.front().Directory == cloneTwo.string());
  assert(commands.front().Filename == (cloneTwo / sourceRelative).string());

  facts::ProjectConfiguration invalid;
  invalid.repositoryName = "cpp-indexer";
  invalid.activeClone.path = cloneTwo.string();
  invalid.components.push_back({.name = "cpp-indexer", .path = "."});
  invalid.files.push_back(
      {.componentPath = "missing", .name = "bad.cpp", .compileOptions = "[]"});
  assert(!files.replaceProjectConfiguration(invalid));
  stored = facts::loadStoredCompilationDatabase(databasePath.string());
  assert(stored && (*stored)->getAllCompileCommands().size() == 1);
}
