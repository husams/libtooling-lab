#include "storage/FactStore.h"
#include "storage/catalog/Database.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace {
#define require(value) do { if (!(value)) { std::fprintf(stderr, "failed line %d\n", __LINE__); std::abort(); } } while (false)
facts::Function symbol(const char *name) {
  facts::Function value{};
  value.usr = "c:@F@" + std::string(name);
  value.qualifiedName = name;
  return value;
}
facts::Variable variable(const char *name, bool local) {
  facts::Variable value{};
  value.usr = name;
  value.qualifiedName = name;
  value.Properties = local ? static_cast<clang::index::SymbolPropertySet>(
      clang::index::SymbolProperty::Local) : 0;
  value.flags |= facts::bit(facts::DefinitionBit);
  value.definition = facts::Region{11, 9};
  return value;
}
std::int64_t count(const std::string &path, const std::string &sql) {
  auto opened = facts::storage::Database::open(path);
  require(opened);
  for (const auto &row : opened->rows(sql)) return row.integer(0);
  std::abort();
}
}
int main(int argc, char **argv) {
  require(argc == 2);
  const auto path = std::filesystem::absolute(argv[1]).string();
  std::filesystem::remove(path);
  const std::array<facts::FileId, 1> selected{1};
  facts::SymbolId retained;
  {
    facts::FactStore store(path);
    auto id = store.save(1, symbol("retained"));
    require(id);
    retained = *id;
    require(store.save(1, symbol("removed")));
    require(store.save(2, symbol("unselected")));
    require(store.save(1, variable("removed_global", false)));
    require(store.save(1, variable("historical_local", true)));
  }
  {
    facts::FactStore store(path);
    require(store.begin());
    require(store.beginSymbolRefresh(selected));
    auto id = store.save(1, symbol("retained"));
    require(id && *id == retained);
    // AST traversal revisits reopened namespaces and shared builtin types.
    require(store.save(1, symbol("retained")));
    require(store.save(1, symbol("retained")));
    require(store.save(1, symbol("added")));
    require(store.save(1, symbol("added")));
    require(store.finishSymbolRefresh());
    require(store.end(false));
  }
  require(count(path, "SELECT count(*) FROM symbol") == 4);
  require(count(path, "SELECT count(*) FROM symbol WHERE qualified_name='removed'") == 0);
  require(count(path, "SELECT count(*) FROM symbol WHERE qualified_name='unselected'") == 1);
  require(count(path, "SELECT count(*) FROM symbol WHERE qualified_name='removed_global'") == 0);
  require(count(path, "SELECT count(*) FROM symbol WHERE qualified_name='historical_local' AND is_definition=1") == 1);
  require(count(path, "SELECT count(*) FROM definition JOIN symbol ON id=symbol_id WHERE qualified_name='historical_local' AND definition.offset=11 AND definition.size=9") == 1);
  {
    facts::FactStore store(path);
    require(store.begin());
    require(store.beginSymbolRefresh(selected));
    require(store.save(1, symbol("failed")));
    require(store.finishSymbolRefresh());
    require(store.rollback());
  }
  require(count(path, "SELECT count(*) FROM symbol") == 4);
  require(count(path, "SELECT count(*) FROM symbol WHERE qualified_name='failed'") == 0);
  require(count(path, "SELECT count(*) FROM symbol WHERE qualified_name='retained'") == 1);
}
