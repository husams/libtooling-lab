#include "astcache/Fixture.h"
#include "platform/DriverIncludes.h"
#include "storage/driverprobe/Database.h"

#include <llvm/Support/Program.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
namespace probe = facts::storage::driverprobe;
using ast_cache_database_test::Fixture;

std::string read(const fs::path &path) {
  std::ifstream stream(path);
  return {std::istreambuf_iterator<char>(stream), {}};
}

std::size_t probeCount(const fs::path &root) {
  return std::ranges::count(read(root / "probe-count"), '\n');
}

void writeDriver(const fs::path &root) {
  std::ofstream script(root / "fixture-g++");
  script << "#!/bin/sh\n"
         << "echo probe >> '" << (root / "probe-count").string() << "'\n"
         << "pwd >> '" << (root / "probe-directory").string() << "'\n"
         << "printf '%s\\n' \"$@\" >> '" << (root / "probe-options").string() << "'\n"
         << "test ! -f '" << (root / "fail").string() << "' || exit 42\n"
         << "echo '#include <...> search starts here:' >&2\n"
         << "echo ' " << (root / "include/c++/15").string() << "' >&2\n"
         << "echo ' " << (root / "include/target-linux-gnu/c++/15").string() << "' >&2\n"
         << "test ! -f '" << (root / "incomplete").string() << "' || exit 0\n"
         << "echo 'End of search list.' >&2\n";
  script.close();
  fs::permissions(root / "fixture-g++", fs::perms::owner_all);
}

int child(int argc, char **argv) {
  assert(argc == 8);
  const fs::path root(argv[2]);
  const fs::path project(argv[3]);
  const fs::path directory(argv[4]);
  const std::string sysroot(argv[5]);
  std::vector<std::string> arguments{(root / "fixture-g++").string(), "source.cpp"};
  if (!sysroot.empty())
    arguments.push_back("--sysroot=" + sysroot);
  clang::tooling::CompileCommand command(directory.string(), "source.cpp", arguments, "");
  auto configured = facts::platform::configureCommand(command, root / "resource", std::nullopt,
      {.enabled = std::string(argv[6]) == "enabled", .database = project});
  const bool expected = std::string(argv[7]) == "success";
  assert(configured.has_value() == expected);
  if (configured) {
    const auto &result = configured->CommandLine;
    const auto generic = std::ranges::find(result, (root / "include/c++/15").string());
    const auto target = std::ranges::find(result, (root / "include/target-linux-gnu/c++/15").string());
    assert(generic != result.end() && target != result.end() && generic < target);
  }
  return 0;
}

void runChild(const fs::path &executable, const Fixture &fixture,
               const fs::path &directory, const std::string &sysroot = {},
               bool enabled = true, bool success = true) {
  const std::vector<std::string> owned{
      executable.string(), "--child", fixture.root.string(), fixture.path.string(),
      directory.string(), sysroot, enabled ? "enabled" : "disabled",
      success ? "success" : "failure"};
  std::vector<llvm::StringRef> arguments(owned.begin(), owned.end());
  assert(llvm::sys::ExecuteAndWait(executable.string(), arguments) == 0);
}

void testProcesses(const fs::path &executable) {
  Fixture fixture;
  writeDriver(fixture.root);
  const auto originalCpath = std::getenv("CPATH");
  const std::optional<std::string> savedCpath =
      originalCpath ? std::optional<std::string>(originalCpath) : std::nullopt;
  const auto restoreCpath = [&] {
    if (savedCpath)
      assert(setenv("CPATH", savedCpath->c_str(), 1) == 0);
    else
      assert(unsetenv("CPATH") == 0);
  };

  auto absent = probe::read(fixture.path, "absent");
  assert(absent && !*absent);
  assert(fixture.number("SELECT COUNT(*) FROM sqlite_master WHERE name='driver_include_probe'") == 0);
  runChild(executable, fixture, fixture.root);
  runChild(executable, fixture, fixture.root);
  assert(probeCount(fixture.root) == 1);
  const auto originalDirectory = fs::current_path();
  fs::create_directories(fixture.root / "nested");
  fs::current_path(fixture.root / "nested");
  runChild(executable, fixture, fixture.root);
  fs::current_path(originalDirectory);
  assert(probeCount(fixture.root) == 1);
  assert(read(fixture.root / "probe-directory") == fixture.root.string() + "\n");

  std::ofstream(fixture.root / "fixture-g++", std::ios::app) << "# toolchain replacement\n";
  runChild(executable, fixture, fixture.root);
  assert(probeCount(fixture.root) == 2);
  runChild(executable, fixture, fixture.root, "relative-root");
  runChild(executable, fixture, fixture.root, "relative-root");
  assert(probeCount(fixture.root) == 3);
  assert(read(fixture.root / "probe-options").find(
      "--sysroot=" + (fixture.root / "relative-root").string()) != std::string::npos);

  assert(setenv("CPATH", "relative-search-path", 1) == 0);
  runChild(executable, fixture, fixture.root, "relative-root");
  runChild(executable, fixture, fixture.root, "relative-root");
  assert(probeCount(fixture.root) == 4);
  restoreCpath();
  runChild(executable, fixture, fixture.root, "relative-root");
  assert(probeCount(fixture.root) == 4);
  runChild(executable, fixture, fixture.root / "nested", "relative-root");
  assert(probeCount(fixture.root) == 5);

  fixture.sql("DELETE FROM driver_include_path WHERE position=1");
  runChild(executable, fixture, fixture.root);
  assert(probeCount(fixture.root) == 6);
  fixture.sql("UPDATE driver_include_path SET path=path||'corrupt' WHERE position=0");
  runChild(executable, fixture, fixture.root);
  assert(probeCount(fixture.root) == 7);
  const auto rows = fixture.number("SELECT COUNT(*) FROM driver_include_probe");
  runChild(executable, fixture, fixture.root, {}, false);
  runChild(executable, fixture, fixture.root, {}, false);
  assert(probeCount(fixture.root) == 9);
  assert(fixture.number("SELECT COUNT(*) FROM driver_include_probe") == rows);

  assert(setenv("CPATH", "failure-context", 1) == 0);
  std::ofstream(fixture.root / "fail") << "fail";
  runChild(executable, fixture, fixture.root, {}, true, false);
  assert(probeCount(fixture.root) == 10);
  assert(fixture.number("SELECT COUNT(*) FROM driver_include_probe") == rows);
  fs::remove(fixture.root / "fail");
  runChild(executable, fixture, fixture.root);
  assert(probeCount(fixture.root) == 11);
  assert(setenv("CPATH", "incomplete-context", 1) == 0);
  std::ofstream(fixture.root / "incomplete") << "incomplete";
  runChild(executable, fixture, fixture.root, {}, true, false);
  assert(probeCount(fixture.root) == 12);
  fs::remove(fixture.root / "incomplete");
  runChild(executable, fixture, fixture.root);
  assert(probeCount(fixture.root) == 13);
  restoreCpath();
}

void testStorage() {
  Fixture fixture;
  const probe::IncludePaths includes{fixture.root / "include/c++/15",
                                     fixture.root / "include/target/c++/15"};
  assert(!probe::write(fixture.root / "missing.db", "missing", includes));
  assert(!fs::exists(fixture.root / "missing.db"));
  assert(!probe::write(fixture.path, "empty", {}));
  for (int index = 0; index < 140; ++index)
    assert(probe::write(fixture.path, "context-" + std::to_string(index), includes));
  assert(fixture.number("SELECT COUNT(*) FROM driver_include_probe") == 140);
  for (int index = 0; index < 140; ++index) {
    auto stored = probe::read(fixture.path, "context-" + std::to_string(index));
    assert(stored && *stored && **stored == includes);
  }
  fixture.sql("CREATE TRIGGER reject_probe_path BEFORE INSERT ON driver_include_path "
              "BEGIN SELECT RAISE(ABORT,'injected write failure'); END");
  assert(!probe::write(fixture.path, "context-0", {fixture.root / "replacement"}));
  auto retained = probe::read(fixture.path, "context-0");
  assert(retained && *retained && **retained == includes);
}

} // namespace

int main(int argc, char **argv) {
  if (argc > 1)
    return child(argc, argv);
  testProcesses(fs::absolute(argv[0]));
  testStorage();
}
