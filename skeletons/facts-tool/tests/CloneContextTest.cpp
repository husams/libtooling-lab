#include "storage/CloneContext.h"
#include "storage/FileManager.h"
#include "storage/catalog/Component.h"
#include "storage/catalog/File.h"
#include "storage/catalog/Repository.h"
#include "tooling/StoredCompilationDatabase.h"
#include "tooling/CompilationCommandCodec.h"
#include "tooling/CompilationPathRemapping.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <future>
namespace {
#define require(value) do { if (!(value)) { std::fprintf(stderr, "failed line %d\n", __LINE__); std::abort(); } } while (false)
void verify(const std::filesystem::path &database,
            const std::filesystem::path &root, facts::FileId id) {
  const auto source = (root / "source.cpp").string();
  const std::vector<std::string> sources{source};
  auto loaded = facts::loadStoredCompilationDatabase(database.string(), sources);
  require(loaded);
  const auto commands = (*loaded)->getAllCompileCommands();
  require(commands.size() == 1);
  require(commands.front().Filename == source);
  require(commands.front().Directory == root.string());
  require(std::ranges::find(commands.front().CommandLine,
                           "-iquote" + (root / "include").string()) !=
          commands.front().CommandLine.end());
  require(std::ranges::find(commands.front().CommandLine,
                           "-I" + (root / "include").string()) !=
          commands.front().CommandLine.end());
  facts::FileManager manager(database.string());
  require(manager.getId(source) == id);
  auto catalog = facts::catalog::open(database.string(), false);
  require(catalog);
  auto files = facts::catalog::files(*catalog);
  require(files && files->size() == 1);
  require(facts::catalog::filePath(files->front()) == root / "source.cpp");
  auto components = facts::catalog::components(*catalog);
  require(components);
  const auto fixture = std::ranges::find_if(*components,
      [](const auto &value) { return value.value.name == "fixture"; });
  require(fixture != components->end() && facts::catalog::componentRoot(*fixture) == root);
}
} // namespace
int main(int argc, char **argv) {
  require(argc == 2);
  require(facts::remapCompilePath("=/old/include", {"/old", "/new"}) ==
          "=/new/include");
  require(facts::remapCompilePath("/oldest/include", {"/old", "/new"}) ==
          "/oldest/include");
  const auto root = std::filesystem::absolute(argv[1]);
  std::filesystem::remove_all(root);
  const auto active = root / "active", alternate = root / "alternate";
  for (const auto &clone : {active, alternate}) {
    std::filesystem::create_directories(clone / "include");
    std::ofstream(clone / "source.cpp") << "int fixture;\n";
  }
  const auto database = root / "project.db";
  facts::FileManager manager(database.string());
  facts::ProjectConfiguration configuration{
      .repositoryName = "fixture",
      .activeClone = {.path = active.string(), .label = "active"},
      .components = {{.name = "fixture", .path = "."}},
      .files = {{.componentPath = ".", .name = "source.cpp",
                 .driver = "/usr/bin/c++", .workingDirectory = active.string(),
                 .compileOptions = facts::encodeCompileOptions(
                     {"-I<fixture>/include", "-iquote" +
                      (active / "include").string()})}}};
  require(manager.replaceProjectConfiguration(configuration));
  require(manager.addClone("fixture", {.path = alternate.string(),
                                       .label = "alternate"}));
  const auto id = manager.getId((active / "source.cpp").string());
  require(id);
  auto catalog = facts::catalog::open(database.string(), false);
  require(catalog);
  const auto repository = facts::catalog::repository(*catalog, "fixture");
  require(repository);
  const auto clones = facts::catalog::clones(*catalog, repository->id);
  require(clones && clones->size() == 2);
  const auto selected = std::ranges::find(*clones, alternate.string(), &facts::ProjectClone::path);
  require(selected != clones->end());
  verify(database, active, *id);
  {
    facts::ScopedCloneContext scope(*selected);
    require(!manager.getId((active / "source.cpp").string()));
    require(manager.getId((alternate / "source.cpp").string()) == *id);
    verify(database, alternate, *id);
    std::async(std::launch::async, [&] { verify(database, active, *id); }).get();
    {
      facts::ScopedCloneContext reset(std::nullopt);
      verify(database, active, *id);
    }
    verify(database, alternate, *id);
    require(facts::catalog::repository(*catalog, "fixture")->activeCloneId ==
            repository->activeCloneId);
  }
  verify(database, active, *id);
  require(!manager.getId((alternate / "source.cpp").string()));
}
