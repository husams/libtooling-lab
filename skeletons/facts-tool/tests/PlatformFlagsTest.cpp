#include "platform/DriverIncludes.h"
#include "platform/PlatformFlags.h"
#include "platform/ResourceDirectory.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

class FixtureCompilationDatabase final
    : public clang::tooling::CompilationDatabase {
public:
  std::vector<clang::tooling::CompileCommand> commands;

  std::vector<clang::tooling::CompileCommand>
  getCompileCommands(llvm::StringRef file) const override {
    const auto source = std::filesystem::absolute(file.str()).lexically_normal();
    std::vector<clang::tooling::CompileCommand> result;
    for (const auto &command : commands)
      if (std::filesystem::path(command.Filename) == source)
        result.push_back(command);
    if (!result.empty())
      return result;
    for (const auto &command : commands)
      if (std::filesystem::weakly_canonical(command.Filename) ==
          std::filesystem::weakly_canonical(source))
        result.push_back(command);
    return result;
  }
};

void testSourceSelectors(const std::filesystem::path &root) {
  const auto previous = std::filesystem::current_path();
  const auto build = root / "build";
  const auto source = root / "src" / "source.cpp";
  const auto alias = root / "alias.cpp";
  std::filesystem::create_directories(build);
  std::filesystem::create_directories(source.parent_path());
  std::ofstream(source) << "int selected() { return 1; }\n";
  std::filesystem::create_symlink(source, alias);
  std::filesystem::current_path(root);

  FixtureCompilationDatabase input;
  input.commands.emplace_back(build.string(), source.string(),
                               std::vector<std::string>{"clang++", source.string()},
                               "");
  const std::vector<std::string> repeated{
      "src/source.cpp", source.string(), "./src/source.cpp", alias.string()};
  auto configured = facts::configurePlatformCompilationDatabase(input, repeated);
  assert(configured);
  assert((*configured)->getAllCompileCommands().size() == 1);
  for (const auto &selector : repeated) {
    const auto commands = (*configured)->getCompileCommands(selector);
    assert(commands.size() == 1);
    assert(commands.front().Filename == source.string());
  }
  assert((*configured)->getCompileCommands("source.cpp").empty());

  auto alternative = input.commands.front();
  alternative.CommandLine.insert(alternative.CommandLine.begin() + 1,
                                   "-DALTERNATIVE=1");
  input.commands.push_back(alternative);
  configured = facts::configurePlatformCompilationDatabase(input, repeated);
  assert(configured);
  assert((*configured)->getAllCompileCommands().size() == 2);
  assert((*configured)->getCompileCommands("src/source.cpp").size() == 2);

  // Separate logical registrations can own different compiler contexts even
  // when the two filenames currently resolve to the same physical file.
  alternative.Filename = alias.string();
  alternative.CommandLine.back() = alias.string();
  input.commands.back() = alternative;
  configured = facts::configurePlatformCompilationDatabase(input, repeated);
  assert(configured);
  assert((*configured)->getAllCompileCommands().size() == 2);
  const auto original = (*configured)->getCompileCommands(source.string());
  const auto aliased = (*configured)->getCompileCommands(alias.string());
  assert(original.size() == 1 && aliased.size() == 1);
  assert(std::ranges::count(original.front().CommandLine, "-DALTERNATIVE=1") == 0);
  assert(std::ranges::count(aliased.front().CommandLine, "-DALTERNATIVE=1") == 1);
  std::filesystem::current_path(previous);
}

void writeBuiltinHeaders(const std::filesystem::path &directory) {
  std::filesystem::create_directories(directory / "include");
  std::ofstream(directory / "include" / "stddef.h") << "/* fixture */\n";
  std::ofstream(directory / "include" / "stdarg.h") << "/* fixture */\n";
}

std::size_t count(const std::vector<std::string> &arguments,
                  const std::string &value) {
  return std::ranges::count(arguments, value);
}

void testResourceLayouts(const std::filesystem::path &root) {
  using facts::platform::resolveResourceDirectory;
  using facts::platform::ResourceDirectoryRequest;
  const auto library = root / "install" / "libexec" / "libclang-cpp.so";
  const auto computed = root / "computed" / "clang" / "22";
  const std::vector<std::filesystem::path> layouts{
      computed,
      library.parent_path() / "clang" / "22",
      library.parent_path() / ".." / "lib" / "clang" / "22",
      library.parent_path() / ".." / "lib64" / "clang" / "22",
  };
  for (const auto &layout : layouts) {
    std::filesystem::remove_all(root);
    writeBuiltinHeaders(layout);
    auto resolved = resolveResourceDirectory(
        ResourceDirectoryRequest{library, computed, 22});
    assert(resolved);
    assert(resolved->lexically_normal() == layout.lexically_normal());
  }
  std::filesystem::remove_all(root);
  auto missing =
      resolveResourceDirectory(ResourceDirectoryRequest{library, computed, 22});
  assert(!missing);
  for (const auto &layout : layouts)
    assert(missing.error().find(layout.lexically_normal().string()) !=
           std::string::npos);
}

void testGnuDriverConfiguration(const std::filesystem::path &root) {
  std::filesystem::create_directories(root);
  const auto driver = root / "fixture-g++";
  const auto discovered = root / "include" / "c++" / "15";
  const auto existing = root / "include" / "c++" / "15" / "target";
  std::filesystem::create_directories(discovered);
  std::filesystem::create_directories(existing);
  {
    std::ofstream script(driver);
    script << "#!/bin/sh\n"
              "echo '#include <...> search starts here:' >&2\n"
           << "echo ' " << discovered.string() << "' >&2\n"
           << "echo ' " << existing.string()
           << "' >&2\n"
              "echo 'End of search list.' >&2\n"
              "exit 0\n";
  }
  std::filesystem::permissions(driver, std::filesystem::perms::owner_exec |
                                           std::filesystem::perms::owner_read |
                                           std::filesystem::perms::owner_write);
  const auto resource = root / "resource";
  const auto source = root / "source.cpp";
  clang::tooling::CompileCommand command(
      root.string(), source.string(),
      {driver.string(), "--target=x86_64-linux-gnu",
       "--gcc-toolchain=/toolchain", "--sysroot=/sysroot", "-stdlib=libstdc++",
       "-isystem", existing.string(), source.string(), "-Werror"},
      "");
  auto configured =
      facts::platform::configureCommand(command, resource, std::nullopt);
  assert(configured);
  const auto &arguments = configured->CommandLine;
  assert(arguments.front() == driver.string());
  assert(count(arguments, "--target=x86_64-linux-gnu") == 1);
  assert(count(arguments, "--gcc-toolchain=/toolchain") == 1);
  assert(count(arguments, "--sysroot=/sysroot") == 1);
  assert(count(arguments, "-stdlib=libstdc++") == 1);
  assert(count(arguments, "-Werror") == 1);
  assert(count(arguments, existing.string()) == 1);
  assert(count(arguments, discovered.string()) == 1);
  assert(count(arguments, "-resource-dir") == 1);
  assert(count(arguments, resource.string()) == 1);

  std::ofstream(driver, std::ios::trunc) << "#!/bin/sh\nexit 42\n";
  auto changed =
      facts::platform::configureCommand(command, resource, std::nullopt);
  assert(!changed);
  assert(changed.error().find("exit 42") != std::string::npos);
}

} // namespace

int main(int argc, char **argv) {
  assert(argc == 2);
  const auto root = std::filesystem::absolute(argv[1]);
  std::filesystem::remove_all(root);
  testResourceLayouts(root / "resources");
  testGnuDriverConfiguration(root / "driver");
  testSourceSelectors(root / "selectors");
  std::filesystem::remove_all(root);
}
