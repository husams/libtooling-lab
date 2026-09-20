#include "commands/CompilationDatabase.h"
#include "platform/DriverIncludes.h"
#include "tooling/StoredCompilationDatabase.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace {
#define require(value) do { if (!(value)) { std::fprintf(stderr, "failed line %d\n", __LINE__); std::abort(); } } while (false)
void beforeSeparator(const std::vector<std::string> &arguments, const char *option) {
  const auto found = std::ranges::find(arguments, option);
  const auto separator = std::ranges::find(arguments, "--");
  require(found != arguments.end() && separator != arguments.end() && found < separator);
}
}
int main() {
  const std::string header = "/project/include/common.hpp";
  clang::tooling::CompileCommand original{
      "/project", "/project/src/main.cpp",
      {"clang++", "-std=c++20", "-resource-dir", "/old/resource", "/project/src/main.cpp"}, ""};
  auto command = clang::tooling::transferCompileCommand(original, header);
  require(command.CommandLine.back() == header);
  auto database = facts::commands::appendExtraArguments(
      facts::makeStoredCompilationDatabase({command}), {"-DHEADER_OPTION=1", "-I/extra/include"});
  const auto commands = database->getCompileCommands(header);
  require(commands.size() == 1);
  auto configured = facts::platform::configureCommand(
      commands.front(), "/new/resource", "/sdk/root");
  require(configured && configured->Filename == header);
  const auto &arguments = configured->CommandLine;
  require(arguments.back() == header && arguments[arguments.size() - 2] == "--");
  beforeSeparator(arguments, "-DHEADER_OPTION=1");
  beforeSeparator(arguments, "-I/extra/include");
  beforeSeparator(arguments, "-resource-dir");
  beforeSeparator(arguments, "-isysroot");
  require(std::ranges::count(arguments, "-resource-dir") == 1);
  require(std::ranges::find(arguments, "/old/resource") == arguments.end());
  require(std::ranges::find(arguments, "/new/resource") != arguments.end());
  require(std::ranges::find(arguments, "/sdk/root") != arguments.end());
  original.CommandLine.push_back("--");
  original.CommandLine.push_back(header);
  auto unchanged = facts::commands::appendExtraArguments(
      facts::makeStoredCompilationDatabase({original}), {});
  require(unchanged->getCompileCommands(original.Filename).front().CommandLine == original.CommandLine);
}
