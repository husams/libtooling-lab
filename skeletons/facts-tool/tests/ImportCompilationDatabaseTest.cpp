#include "tooling/ImportCompilationDatabase.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main(int argc, char **argv) {
  assert(argc == 2);
  const auto root = std::filesystem::absolute(argv[1]);
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / "build");
  for (const std::string name : {"one", "two"}) {
    std::filesystem::create_directories(root / "src" / name);
    std::ofstream(root / "src" / name / "main.cpp") << "int value;\n";
    std::ofstream(root / "src" / name / "flags.rsp") << "-D" << name << " -Iinclude\n";
  }
  std::filesystem::create_symlink(root / "src/one/main.cpp", root / "src/two/alias.cpp");
  std::ofstream(root / "build/compile_commands.json") << R"([
    {"directory":"../src/one","file":"main.cpp",
     "arguments":["clang++","@flags.rsp","-c","main.cpp"]},
    {"directory":"../src/two","file":"main.cpp",
     "arguments":["clang++","@flags.rsp","-c","main.cpp"]},
    {"directory":"../src/two","file":"alias.cpp",
     "arguments":["clang++","-DALIAS","-c","alias.cpp"]}
  ])";
  const auto before = std::filesystem::current_path();
  auto database = facts::loadImportCompilationDatabase(root / "build");
  assert(database);
  assert((*database)->getAllFiles().size() == 3);
  for (const std::string name : {"one", "two"}) {
    const auto directory = root / "src" / name;
    auto commands = (*database)->getCompileCommands((directory / "main.cpp").string());
    assert(commands.size() == 1);
    assert(commands.front().Directory == directory.string());
    assert(commands.front().Filename == (directory / "main.cpp").string());
    assert(std::ranges::find(commands.front().CommandLine, "-D" + name) !=
           commands.front().CommandLine.end());
    assert(std::ranges::find(commands.front().CommandLine, "@flags.rsp") ==
           commands.front().CommandLine.end());
    assert(std::ranges::find(commands.front().CommandLine, "--driver-mode=g++") !=
           commands.front().CommandLine.end());
  }
  const auto alias = (*database)->getCompileCommands((root / "src/two/alias.cpp").string());
  assert(alias.size() == 1);
  assert(std::ranges::find(alias.front().CommandLine, "-DALIAS") != alias.front().CommandLine.end());
  assert(std::filesystem::current_path() == before);

  // Preserve Clang's normal fallback for selected headers/unlisted sources.
  // The importer still validates whether the selected file actually exists.
  const auto missing = (root / "src/one/missing.cpp").string();
  const auto inferred = (*database)->getCompileCommands(missing);
  assert(inferred.size() == 1 && inferred.front().Filename == missing);
  assert(inferred.front().Directory == (root / "src/one").string());
  assert(std::ranges::find(inferred.front().CommandLine, "-Done") !=
         inferred.front().CommandLine.end());

  // Keep the existing CLI path for compile_flags.txt with explicit sources.
  std::filesystem::create_directory(root / "fixed");
  std::ofstream(root / "fixed/compile_flags.txt") << "-DFIXED\n";
  database = facts::loadImportCompilationDatabase(root / "fixed");
  assert(database);
  const auto fixed = (*database)->getCompileCommands((root / "src/one/main.cpp").string());
  assert(fixed.size() == 1);
  assert(std::ranges::find(fixed.front().CommandLine, "-DFIXED") != fixed.front().CommandLine.end());
  std::filesystem::remove_all(root);
}
