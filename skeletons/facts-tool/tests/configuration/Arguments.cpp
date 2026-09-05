#include "Sandbox.h"
#include "commands/ConfigurationSupport.h"
#include "commands/CompilationViews.h"

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
}
}
