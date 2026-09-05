#include "Sandbox.h"
#include "commands/ConfigurationSupport.h"
#include "commands/CompilationViews.h"
#include <clang/Tooling/JSONCompilationDatabase.h>

namespace configuration_test {
void arguments() {
  const std::vector<std::string> defaults{"-include", "space header.hpp", "-DVALUE=2"};
  const std::vector<std::string> fragments{"-DVALUE=3 '-DSPACE=two words'", "-DVALUE=3"};
  const auto args = facts::commands::mergedArguments(defaults, fragments);
  const std::vector<std::string> expected{"-include", "space header.hpp", "-DVALUE=2",
      "-DVALUE=3", "-DSPACE=two words", "-DVALUE=3"};
  assert(args && *args == expected);
  assert(!facts::commands::mergedArguments({}, {"'unterminated"}));
  assert(!facts::commands::mergedArguments({}, {"trailing\\"}));
  auto literal = facts::commands::mergedArguments({}, {"'$HOME' '*.cpp'"});
  assert(literal && *literal == std::vector<std::string>({"$HOME", "*.cpp"}));
  auto empty = facts::commands::mergedArguments({}, {});
  assert(empty && empty->empty());
  const std::vector<std::string> cli{"-DVALUE=3"};
  auto views = facts::commands::compilationViews(
      std::make_unique<clang::tooling::FixedCompilationDatabase>("/work",
          std::vector<std::string>{"-DVALUE=1"}), defaults, cli);
  const auto stored = views.stored->getCompileCommands("unit.cpp")[0].CommandLine;
  const auto applied = views.applied->getCompileCommands("unit.cpp")[0].CommandLine;
  assert(std::vector<std::string>(stored.end() - 1, stored.end()) == cli);
  assert(std::vector<std::string>(applied.end() - 4, applied.end()) ==
         std::vector<std::string>({"-include", "space header.hpp", "-DVALUE=2", "-DVALUE=3"}));
  assert(views.stored->getCompileCommands("unit.cpp")[0].CommandLine == stored);
  std::string error;
  auto json = clang::tooling::JSONCompilationDatabase::loadFromBuffer(R"([
    {"directory":"/work/project", "file":"/work/project/a.cpp",
     "arguments":["clang++","-DJSON_A=1","-I","include space","/work/project/a.cpp"]},
    {"directory":"/work/project/sub", "file":"/work/project/sub/b.cpp",
     "command":"g++ -DJSON_B=1 -include 'base header.hpp' /work/project/sub/b.cpp"}
  ])", error, clang::tooling::JSONCommandLineSyntax::Gnu);
  assert(json && error.empty());
  const auto baseCommands = json->getAllCompileCommands();
  auto jsonViews = facts::commands::compilationViews(std::move(json), defaults, cli);
  for (int repeat = 0; repeat < 3; ++repeat) {
    const auto persisted = jsonViews.stored->getAllCompileCommands();
    const auto runtime = jsonViews.applied->getAllCompileCommands();
    assert(persisted.size() == 2 && runtime.size() == 2);
    for (std::size_t i = 0; i < baseCommands.size(); ++i) {
      auto expectedStored = baseCommands[i].CommandLine;
      expectedStored.insert(expectedStored.end(), cli.begin(), cli.end());
      auto expectedRuntime = baseCommands[i].CommandLine;
      expectedRuntime.insert(expectedRuntime.end(), defaults.begin(), defaults.end());
      expectedRuntime.insert(expectedRuntime.end(), cli.begin(), cli.end());
      assert(persisted[i].CommandLine == expectedStored);
      assert(runtime[i].CommandLine == expectedRuntime);
      assert(runtime[i].Directory == baseCommands[i].Directory);
      assert(persisted[i].Directory == baseCommands[i].Directory);
      const auto selected = jsonViews.applied->getCompileCommands(baseCommands[i].Filename);
      assert(selected.size() == 1 && selected[0].CommandLine == expectedRuntime);
      assert(selected[0].Directory == baseCommands[i].Directory);
    }
  }
}
}
