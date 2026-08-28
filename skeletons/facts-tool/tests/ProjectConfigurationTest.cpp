#include "storage/FileManager.h"
#include "tooling/ProjectImport.h"
#include "tooling/StoredCompilationDatabase.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <sqlite3.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <ranges>
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

  explicit TestCompilationDatabase(
      std::vector<clang::tooling::CompileCommand> commands)
      : commands_(std::move(commands)) {}

  std::vector<clang::tooling::CompileCommand>
  getCompileCommands(llvm::StringRef filePath) const override {
    auto matching = commands_ | std::views::filter([&](const auto &command) {
                      return filePath == command.Filename;
                    });
    return matching |
           std::ranges::to<std::vector<clang::tooling::CompileCommand>>();
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
  auto preferred = clang::tooling::CompileCommand{
      cloneOne.string(),
      source.string(),
      {"clang++", "-std=c++23", "-Itests/fixtures", "-DVALUE=1",
       source.string()},
      ""};
  auto duplicate = preferred;
  duplicate.CommandLine[3] = "-DVALUE=2";
  TestCompilationDatabase input(
      std::vector<clang::tooling::CompileCommand>{duplicate, preferred});
  const std::vector<std::string> fallbackSources{source.string()};
  auto imported = facts::importProjectConfiguration(
      files, input, fallbackSources,
      facts::ProjectImportOptions{.repositoryName = "facts-tool"});
  assert(imported && imported->importedFiles == 1);
  assert(imported->duplicateCommands == 1);
  assert(imported->diagnostics.size() == 1);
  assert(imported->diagnostics.front().contains("duplicate compile command"));
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

  // Storing compile commands is not registering files. Only an import that
  // finished registering may call the registry complete, and storing the
  // configuration again withdraws the claim.
  auto status = files.registryStatus();
  assert(status && !status->complete);
  assert(files.markRegistryComplete("toolchain-under-test"));
  status = files.registryStatus();
  assert(status && status->complete);
  assert(status->fingerprint == "toolchain-under-test");
  assert(status->fileCount == 1);
  assert(facts::importProjectConfiguration(files, input, fallbackSources,
                                           facts::ProjectImportOptions{}));
  status = files.registryStatus();
  assert(status && !status->complete && status->fingerprint.empty());

  auto stored = facts::loadStoredCompilationDatabase(databasePath.string());
  assert(stored);
  auto commands = (*stored)->getCompileCommands(source.string());
  assert(commands.size() == 1);
  assert(commands.front().Directory == cloneOne.string());
  assert(commands.front().Filename == source.string());
  const std::vector<std::string> expectedArguments = {
      "clang++", "-std=c++23", "-I" + (cloneOne / "tests/fixtures").string(),
      "-DVALUE=1", source.string()};
  assert(commands.front().CommandLine == expectedArguments);

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

  auto invalidOptions = facts::ProjectImportOptions{};
  invalidOptions.components = {
      facts::ProjectComponent{.name = "first", .path = "."},
      facts::ProjectComponent{.name = "second", .path = "."}};
  const auto rejected = facts::importProjectConfiguration(
      files, input, fallbackSources, invalidOptions);
  assert(!rejected);
  assert(rejected.error().contains("is configured twice"));
  stored = facts::loadStoredCompilationDatabase(databasePath.string());
  assert(stored);
  commands = (*stored)->getAllCompileCommands();
  assert(commands.size() == 1);
  assert(commands.front().Directory == cloneTwo.string());
  assert(commands.front().Filename == (cloneTwo / sourceRelative).string());

  // Every rejection the storage boundary makes names the field it refused,
  // so a caller can report the cause instead of "Invalid argument".
  const auto valid = [&] {
    facts::ProjectConfiguration configuration;
    configuration.repositoryName = "cpp-indexer";
    configuration.activeClone.path = cloneTwo.string();
    configuration.components.push_back({.name = "cpp-indexer", .path = "."});
    configuration.files.push_back(
        {.componentPath = ".", .name = "good.cpp", .compileOptions = "[]"});
    return configuration;
  };
  const auto refusal = [&](facts::ProjectConfiguration configuration) {
    auto replaced = files.replaceProjectConfiguration(configuration);
    assert(!replaced);
    return std::move(replaced).error();
  };

  auto emptyClone = valid();
  emptyClone.activeClone.path.clear();
  assert(refusal(emptyClone).contains("active clone path is empty"));

  auto emptyRepository = valid();
  emptyRepository.repositoryName.clear();
  assert(refusal(emptyRepository).contains("repository name is empty"));

  auto noComponents = valid();
  noComponents.components.clear();
  noComponents.files.clear();
  assert(refusal(noComponents).contains("no components"));

  auto emptyComponentPath = valid();
  emptyComponentPath.components.front().path.clear();
  assert(refusal(emptyComponentPath).contains("has an empty path"));

  auto repeatedComponent = valid();
  repeatedComponent.components.push_back({.name = "twin", .path = "."});
  assert(refusal(repeatedComponent).contains("is configured twice"));

  auto unnamedFile = valid();
  unnamedFile.files.front().name.clear();
  assert(refusal(unnamedFile).contains("has an empty name"));

  auto unknownComponent = valid();
  unknownComponent.files.front().componentPath = "missing";
  const auto unknown = refusal(unknownComponent);
  assert(unknown.contains("unconfigured component"));
  assert(unknown.contains("missing"));

  auto withoutOptions = valid();
  withoutOptions.files.front().compileOptions.clear();
  assert(refusal(withoutOptions).contains("no compile options"));

  stored = facts::loadStoredCompilationDatabase(databasePath.string());
  assert(stored && (*stored)->getAllCompileCommands().size() == 1);
}
