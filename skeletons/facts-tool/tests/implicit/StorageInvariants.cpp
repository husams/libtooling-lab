#include "storage/FactStore.h"
#include <clang/AST/Type.h>
#include <filesystem>
#include <iostream>

namespace {
void require(bool valid) {
  if (!valid)
    throw std::runtime_error("compiler symbol storage invariant failed");
}

facts::Symbol symbol(std::string usr) {
  facts::Symbol value{};
  value.usr = std::move(usr);
  value.qualifiedName = value.usr;
  return value;
}

void execute(const std::string &path, const char *sql) {
  sqlite3 *db = nullptr;
  require(sqlite3_open(path.c_str(), &db) == SQLITE_OK);
  const auto result = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
  sqlite3_close(db);
  require(result == SQLITE_OK);
}

void check(const std::string &path, bool primitiveFirst) {
  std::filesystem::remove(path);
  facts::SymbolId compiler;
  const auto primitive =
      facts::SymbolId{0, static_cast<unsigned>(clang::BuiltinType::Int) + 1};
  const auto callable = symbol("caller");
  {
    facts::FactStore store(path);
    const auto caller = store.save(1, callable).value();
    const auto savePrimitive = [&] {
      require(
          store.saveReturnType(caller, {primitive, "int", "int"}).has_value());
    };
    if (primitiveFirst)
      savePrimitive();
    compiler = store.save(0, symbol("compiler")).value();
    require(compiler.index >
            static_cast<unsigned>(clang::BuiltinType::LastKind) + 1);
    savePrimitive();
    require(store.load<facts::Symbol>(compiler)->usr == "compiler");
    require(store.load<facts::Symbol>(primitive)->usr.starts_with("c:@BT@"));
  }
  // Simulate occupied dynamic IDs in an upgraded DB with a missing allocator.
  execute(path, "UPDATE symbol SET id=4000000000 WHERE usr='compiler';"
                "DELETE FROM symbol_allocator WHERE file_id=0;");
  {
    facts::FactStore reopened(path);
    require(reopened.save(0, symbol("next"))->index == 4000000001U);
    require(reopened.save(0, symbol("compiler"))->index == 4000000000U);
    require(reopened.load<facts::Symbol>(primitive)->usr.starts_with("c:@BT@"));
  }
  execute(path,
          "UPDATE symbol_allocator SET next_index=4100000000 WHERE file_id=0;");
  {
    facts::FactStore reopened(path);
    require(reopened.save(0, symbol("high-water"))->index == 4100000000U);
    require(reopened.begin().has_value());
    require(reopened.save(0, symbol("rolled-back")).has_value());
    require(reopened.rollback().has_value());
  }
  execute(path,
          "UPDATE symbol_allocator SET next_index=4294967296 WHERE file_id=0;");
  facts::FactStore exhausted(path);
  auto failed = exhausted.save(0, symbol("overflow"));
  require(!failed && failed.error() == std::errc::value_too_large);
  require(!exhausted.findId("rolled-back").value());
  require(!exhausted.findId("overflow").value());
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  try {
    check(std::string(argv[1]) + "-first.db", true);
    check(std::string(argv[1]) + "-last.db", false);
    std::cout << "primitive orders, occupied IDs, reopen, high-water, "
                 "rollback, overflow: pass\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
