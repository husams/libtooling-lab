#include "storage/FileManager.h"
#include "tooling/ProjectImport.h"
#include "tooling/StoredCompilationDatabase.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

namespace {

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
  const auto cloneOne = std::filesystem::canonical(argv[2]);
  const auto cloneTwo = databasePath.parent_path() / "clone-two";
  const auto sourceRelative =
      std::filesystem::path("tests/fixtures/tu_one.cpp");
  std::filesystem::create_directories(cloneTwo / sourceRelative.parent_path());
  std::filesystem::copy_file(cloneOne / sourceRelative,
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
      files, input, fallbackSources, facts::ProjectImportOptions{});
  assert(imported && imported->importedFiles == 1);

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
      "facts-tool",
      facts::ProjectClone{.path = cloneTwo.string(), .label = "second"}));
  assert(files.switchActiveClone("facts-tool", "second"));
  stored = facts::loadStoredCompilationDatabase(databasePath.string());
  assert(stored);
  commands = (*stored)->getAllCompileCommands();
  assert(commands.size() == 1);
  assert(commands.front().Directory == cloneTwo.string());
  assert(commands.front().Filename == (cloneTwo / sourceRelative).string());

  facts::ProjectConfiguration invalid;
  invalid.repositoryName = "facts-tool";
  invalid.activeClone.path = cloneTwo.string();
  invalid.components.push_back({.name = "facts-tool", .path = "."});
  invalid.files.push_back(
      {.componentPath = "missing", .name = "bad.cpp", .compileOptions = "[]"});
  assert(!files.replaceProjectConfiguration(invalid));
  stored = facts::loadStoredCompilationDatabase(databasePath.string());
  assert(stored && (*stored)->getAllCompileCommands().size() == 1);
}
