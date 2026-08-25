#include "platform/DriverIncludes.h"
#include "platform/ResourceDirectory.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

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
  auto cached =
      facts::platform::configureCommand(command, resource, std::nullopt);
  assert(cached);
  assert(count(cached->CommandLine, discovered.string()) == 1);
}

} // namespace

int main(int argc, char **argv) {
  assert(argc == 2);
  const auto root = std::filesystem::absolute(argv[1]);
  std::filesystem::remove_all(root);
  testResourceLayouts(root / "resources");
  testGnuDriverConfiguration(root / "driver");
  std::filesystem::remove_all(root);
}
