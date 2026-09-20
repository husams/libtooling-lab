#include "storage/FileManager.h"
#include "storage/catalog/Repository.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {
#define require(value) do { if (!(value)) { std::fprintf(stderr, "failed line %d\n", __LINE__); std::abort(); } } while (false)
void imported(facts::FileManager &manager, const std::string &name,
              const std::filesystem::path &clone) {
  require(manager.replaceProjectConfiguration({
      .repositoryName = name,
      .activeClone = {.path = clone.string(), .label = "active"},
      .components = {{.name = name, .path = "."}},
      .files = {{.componentPath = ".", .name = "source.cpp",
                 .compileOptions = "[]"}}}));
}
void indexed(facts::catalog::Database &database, const std::string &repository,
              bool expected) {
  auto rows = facts::catalog::query(database,
      "SELECT f.indexed FROM file f JOIN directory d ON d.id=f.directory_id "
      "JOIN component c ON c.id=d.component_id JOIN repository r ON "
      "r.id=c.repository_id WHERE r.name=?1",
      [](const facts::storage::Row &row) { return row.integer(0) != 0; }, repository);
  require(rows && rows->size() == 1 && rows->front() == expected);
}
}

int main(int argc, char **argv) {
  require(argc == 2);
  const auto root = std::filesystem::absolute(argv[1]);
  std::filesystem::remove_all(root);
  const auto active = root / "alpha", alternate = root / "alternate", beta = root / "beta";
  for (const auto &clone : {active, alternate, beta}) {
    std::filesystem::create_directories(clone);
    std::ofstream(clone / "source.cpp") << "int fixture;\n";
  }
  facts::FileManager manager((root / "project.db").string());
  imported(manager, "alpha", active);
  imported(manager, "beta", beta);
  require(manager.addClone("alpha", {.path = alternate.string(), .label = "alternate"}));
  auto opened = facts::catalog::open((root / "project.db").string(), true);
  require(opened);
  auto &database = *opened;
  const auto alpha = facts::catalog::repository(database, "alpha");
  require(alpha);
  require(facts::catalog::execute(database, "UPDATE file SET indexed=1"));
  require(manager.switchActiveClone("alpha", "active"));
  indexed(database, "alpha", true);
  indexed(database, "beta", true);
  require(manager.switchActiveClone("alpha", "alternate"));
  indexed(database, "alpha", false);
  indexed(database, "beta", true);
  require(facts::catalog::execute(database, "UPDATE file SET indexed=1"));
  require(facts::catalog::switchClone(database, *alpha, "alternate"));
  indexed(database, "alpha", true);
  {
    auto transaction = database.write();
    require(transaction);
    require(facts::catalog::switchClone(database, *alpha, "active"));
    require(transaction->commit());
  }
  indexed(database, "alpha", false);
  indexed(database, "beta", true);
  require(facts::catalog::execute(database, "UPDATE file SET indexed=1"));
  require(database.executeScript("CREATE TRIGGER reject_invalidation BEFORE UPDATE "
      "OF indexed ON file BEGIN SELECT RAISE(ABORT,'invalidation failed'); END"));
  require(!manager.switchActiveClone("alpha", "alternate"));
  require(facts::catalog::repository(database, "alpha")->activeCloneId == alpha->activeCloneId);
  indexed(database, "alpha", true);
  indexed(database, "beta", true);
  {
    auto transaction = database.write();
    require(transaction);
    require(!facts::catalog::switchClone(database, *alpha, "alternate"));
    require(transaction->commit());
  }
  require(facts::catalog::repository(database, "alpha")->activeCloneId == alpha->activeCloneId);
  indexed(database, "alpha", true);
  require(facts::catalog::switchClone(database, *alpha, "active"));
  require(database.executeScript("DROP TRIGGER reject_invalidation"));
}
