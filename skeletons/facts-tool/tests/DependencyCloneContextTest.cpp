#include "storage/DependencyDatabase.h"
#include "storage/CloneContext.h"
#include "storage/FactStore.h"
#include "storage/catalog/Database.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace {
#define require(value) do { if (!(value)) { std::fprintf(stderr, "failed line %d\n", __LINE__); std::abort(); } } while (false)
std::int64_t scalar(const std::string &path, const std::string &query) {
  auto database = facts::storage::Database::open(path);
  require(database);
  auto result = facts::catalog::query(*database, query,
      [](const facts::storage::Row &row) { return row.integer(0); });
  require(result && result->size() == 1);
  return result->front();
}
void seed(const std::string &path, facts::FileId id, const std::string &name) {
  facts::FactStore store(path);
  facts::Function symbol{};
  symbol.usr = "c:@F@" + name;
  symbol.qualifiedName = name;
  require(store.save(id, symbol));
}
} // namespace

int main(int argc, char **argv) {
  require(argc == 2);
  const auto path = std::filesystem::absolute(argv[1]).string();
  std::filesystem::remove(path);
  const std::array<facts::FileId, 1> sources{1};
  facts::ScopedCloneContext outer(std::nullopt);
  const std::array edges{facts::DependencyEdge{1, 2}};
  std::array provenance{
      facts::storage::FactProvenance{1, "/active/source.cpp", "fixture", {}},
      facts::storage::FactProvenance{2, "/shared/header.hpp", "fixture", {}}};
  require(facts::replaceDependencies(path, sources, edges, provenance));
  seed(path, 1, "source_symbol");
  seed(path, 2, "shared_symbol");
  require(facts::replaceDependencies(path, sources, edges, provenance));
  require(scalar(path, "SELECT count(*) FROM symbol") == 2);
  require(outer.refreshedFiles().empty());
  provenance[0].path = "/alternate/source.cpp";
  provenance[0].aliases = {"/active/source.cpp"};
  {
    facts::ScopedCloneContext inner(std::nullopt);
    require(facts::replaceDependencies(path, sources, edges, provenance));
    require(inner.refreshedFiles().size() == 1 && inner.refreshedFiles()[0] == 1);
  }
  require(outer.refreshedFiles().empty());
  require(scalar(path, "SELECT count(*) FROM symbol") == 1);
  require(scalar(path, "SELECT count(*) FROM symbol WHERE qualified_name="
                       "'shared_symbol'") == 1);
  require(scalar(path, "SELECT count(*) FROM facts_project_provenance WHERE "
                       "file_id=1 AND path='/alternate/source.cpp'") == 1);
  require(scalar(path, "SELECT count(*) FROM include_dependency WHERE "
                       "src_file_id=1 AND dst_file_id=2") == 1);
  provenance[0].path = "/unregistered/source.cpp";
  provenance[0].aliases = {"/invalid/source.cpp"};
  require(!facts::replaceDependencies(path, sources, {}, provenance));
  require(scalar(path, "SELECT count(*) FROM include_dependency") == 1);
  require(scalar(path, "SELECT count(*) FROM symbol") == 1);
  require(outer.refreshedFiles().empty());
  provenance[0].path = "/active/source.cpp";
  provenance[0].aliases = {"/alternate/source.cpp"};
  require(facts::replaceDependencies(path, sources, edges, provenance));
  require(outer.refreshedFiles().size() == 1 && outer.refreshedFiles()[0] == 1);
}
