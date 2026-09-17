#include "ast/Indexing.h"
#include "ast/visitors/Traversal.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <sqlite3.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
  if (!condition)
    throw std::runtime_error(std::string{message});
}

int scalar(sqlite3 *database, const std::string &sql) {
  sqlite3_stmt *statement = nullptr;
  require(sqlite3_prepare_v2(database, sql.c_str(), -1, &statement, nullptr) ==
              SQLITE_OK,
          sqlite3_errmsg(database));
  require(sqlite3_step(statement) == SQLITE_ROW, sqlite3_errmsg(database));
  const auto value = sqlite3_column_int(statement, 0);
  sqlite3_finalize(statement);
  return value;
}

void check(const std::filesystem::path &directory) {
  const std::string code = R"cpp(
void target() {}
void alternative() {}
using Pointer = void (*)();
void value_sink(Pointer);
void address_sink(Pointer*);
void reference_sink(Pointer&);
Pointer choose();
Pointer global = target;

void simple() { void (*fp)() = target; fp(); }
void wrappers() { auto fp = &target; (*fp)(); (fp)(); ((fp))(); }
void constant() { const auto fp = target; fp(); }
void read_copy() { auto fp = target; value_sink(fp); fp(); }
void nested(bool condition) { if (condition) { auto fp = target; fp(); } }
void separate() { auto fp = target; auto other = target; other = alternative; fp(); }
void by_value() { auto fp = target; auto closure = [fp] { fp(); }; fp(); }

void parameter(Pointer fp) { fp(); }
void assigned() { auto fp = target; fp = alternative; fp(); }
void later_assignment() { auto fp = target; fp(); fp = alternative; }
void branch(bool condition) { auto fp = target; if (condition) fp = alternative; fp(); }
void address() { auto fp = target; address_sink(&fp); fp(); }
void reference() { auto fp = target; reference_sink(fp); fp(); }
void alias() { auto fp = target; auto &ref = fp; ref = alternative; fp(); }
void conditional_alias(bool condition) {
  auto fp = target; auto other = target;
  auto &ref = condition ? fp : other; ref = alternative; fp();
}
void explicit_alias() {
  auto fp = target; auto &&ref = static_cast<Pointer&&>(fp);
  ref = alternative; fp();
}
void cast_alias() { auto fp = target; const_cast<Pointer&>(fp) = alternative; fp(); }
void assembly_output() { auto fp = target; asm volatile ("" : "+r"(fp)); fp(); }
void by_reference() { auto fp = target; auto closure = [&fp] { fp = alternative; }; closure(); fp(); }
void default_reference() { auto fp = target; auto closure = [&] { fp = alternative; }; closure(); fp(); }
void capture_alias() { auto fp = target; auto closure = [&ref = fp] { ref = alternative; }; closure(); fp(); }
void persistent() { static auto fp = target; fp(); }
void volatile_pointer() { void (*volatile fp)() = target; fp(); }
void conditional(bool condition) { auto fp = condition ? target : alternative; fp(); }
void pointer_copy() { auto first = target; auto fp = first; fp(); }
void dynamic() { auto fp = choose(); fp(); }
void global_pointer() { global(); }
)cpp";
  const auto source = directory / "indirect.cpp";
  std::ofstream(source) << code;
  auto ast = clang::tooling::buildASTFromCodeWithArgs(
      code, {"-std=c++23", "-nostdinc", "-nostdinc++"}, source.string());
  require(ast && !ast->getDiagnostics().hasErrorOccurred(), "fixture parse failed");

  const auto databasePath = directory / "facts.sqlite";
  {
    facts::FileManager files((directory / "registry.sqlite").string());
    require(files.addBulk(std::array{source.string()}).has_value(),
            "cannot register source");
    facts::FactStore store(databasePath.string());
    require(store.begin().has_value(), "cannot start extraction");
    facts::IndexingStatus status;
    facts::traverse(ast->getASTContext(), files, store, status);
    require(status.complete(), "extraction failed");
    require(store.end(false).has_value(), "cannot commit extraction");
  }

  sqlite3 *database = nullptr;
  require(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK,
          "cannot open extracted facts");
  const auto targetSites = [&](std::string_view caller) {
    return scalar(database,
                  "SELECT COUNT(*) FROM relation_site r "
                  "JOIN symbol s ON s.id=r.source_id "
                  "JOIN symbol d ON d.id=r.destination_id "
                  "WHERE r.kind=1 AND d.qualified_name='target' "
                  "AND s.qualified_name='" + std::string{caller} + "'");
  };
  const auto unresolved = [&](std::string_view caller) {
    return scalar(database,
                  "SELECT COUNT(*) FROM callgraph_unresolved_site r "
                  "JOIN symbol s ON s.id=r.source_id "
                  "WHERE s.qualified_name='" + std::string{caller} + "'");
  };
  const auto pointerSites = [&](std::string_view caller) {
    return scalar(database,
                  "SELECT COUNT(*) FROM callgraph_pointer_call_site r "
                  "JOIN symbol s ON s.id=r.source_id "
                  "WHERE s.qualified_name='" + std::string{caller} + "'");
  };

  for (const auto name : {"simple", "constant", "read_copy", "nested",
                          "separate", "by_value"}) {
    require(targetSites(name) == 1, std::string{name} + " lost exact target");
    require(unresolved(name) == 0,
            std::string{name} + " retained unresolved evidence");
    require(pointerSites(name) == 1,
            std::string{name} + " lost pointer-call evidence");
  }
  require(targetSites("wrappers") == 3 && unresolved("wrappers") == 0,
          "wrapped calls lost exact sites");
  require(pointerSites("wrappers") == 3,
          "wrapped calls lost pointer-call evidence");
  for (const auto name : {"parameter", "assigned", "later_assignment", "branch",
                          "address", "reference", "alias", "conditional_alias",
                          "explicit_alias", "cast_alias", "assembly_output",
                          "by_reference", "default_reference",
                          "capture_alias", "persistent", "volatile_pointer",
                          "conditional", "pointer_copy", "dynamic",
                          "global_pointer"}) {
    require(targetSites(name) == 0, std::string{name} + " guessed an exact target");
    require(unresolved(name) == 0,
            std::string{name} + " retained unresolved evidence");
    require(pointerSites(name) == 1,
            std::string{name} + " lost pointer-call evidence");
  }
  sqlite3_close(database);
}

} // namespace

int main() {
  llvm::SmallString<256> directory;
  if (const auto error =
          llvm::sys::fs::createUniqueDirectory("facts-indirect-calls", directory)) {
    std::cerr << error.message() << '\n';
    return 1;
  }
  int result = 0;
  try {
    check(directory.str().str());
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    result = 1;
  }
  std::filesystem::remove_all(directory.str().str());
  return result;
}
