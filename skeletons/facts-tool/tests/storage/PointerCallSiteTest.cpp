#include "StorageSchemaTestCases.h"
#include "StorageSchemaTestSupport.h"

#include "storage/FactStore.h"
#include "storage/SchemaMigration.h"

#include <array>

namespace storage_schema_test {
namespace {

bool verifyPointerFacts(facts::Storage &storage, sqlite3 *database,
                        facts::SymbolId caller, facts::SymbolId pointer,
                        facts::SymbolId otherPointer) {
  const facts::PointerCallSite named{
      caller, pointer, 1, {.line = 5, .column = 3, .offset = 42},
      "void (*)(int)", "fp"};
  const facts::PointerCallSite expression{
      caller, std::nullopt, 2, {.line = 7, .column = 3, .offset = 70},
      "void (*)(int)", "factory()"};
  auto replacement = named;
  replacement.target = std::nullopt;
  replacement.expression = "factory()";
  const auto relationCount = [&] {
    return scalar(database, "SELECT COUNT(*) FROM relation WHERE kind=24");
  };
  const auto siteCount = [&] {
    return scalar(database, "SELECT COUNT(*) FROM callgraph_pointer_call_site");
  };
  if (!require(storage.addPointerCallSites(std::array{named, expression})
                   .has_value(),
               "cannot store named and expression pointer invocations") ||
      !require(storage.addPointerCallSites(std::array{named}).has_value(),
               "cannot repeat pointer invocation") ||
      !require(siteCount() == 2 && relationCount() == 1,
               "pointer invocation storage is not idempotent") ||
      !require(scalar(database, "SELECT count FROM relation WHERE kind=24") == 1,
               "duplicate pointer invocation inflated relation count") ||
      !require(textScalar(database, "SELECT signature FROM "
                                    "callgraph_pointer_call_site WHERE "
                                    "target_id IS NULL") == "void (*)(int)",
               "expression-only invocation lost its callable signature") ||
      !require(storage.addPointerCallSites(std::array{named, replacement}).has_value(),
               "cannot replace repeated pointer evidence within one batch") ||
      !require(siteCount() == 2 && relationCount() == 0,
               "last pointer observation did not replace the earlier batch relation") ||
      !require(storage.addPointerCallSites(std::array{replacement, named}).has_value(),
               "cannot restore last named pointer observation") ||
      !require(siteCount() == 2 && relationCount() == 1,
               "named last observation did not restore its pointer relation"))
    return false;

  auto invalid = named;
  invalid.signature.clear();
  if (!require(!storage.addPointerCallSites(std::array{invalid}),
               "empty pointer signature was accepted"))
    return false;
  invalid = named;
  invalid.target = facts::SymbolId{99, 1};
  if (!require(!storage.addPointerCallSites(std::array{invalid}),
               "nonexistent pointer declaration was accepted") ||
      !require(siteCount() == 2 && relationCount() == 1,
               "failed update did not roll back pointer evidence") ||
      !require(scalar(database, "SELECT COUNT(*) FROM relation_site "
                               "WHERE kind=24") == 1,
               "failed update lost the previous pointer relation site"))
    return false;

  auto second = named;
  second.location = {.line = 8, .column = 3, .offset = 80};
  if (!require(storage.addPointerCallSites(std::array{second}).has_value(),
               "cannot add second pointer invocation") ||
      !require(scalar(database, "SELECT count FROM relation WHERE kind=24") == 2,
               "distinct pointer invocations did not aggregate"))
    return false;
  second.target = otherPointer;
  second.expression = "other_fp";
  if (!require(storage.addPointerCallSites(std::array{second}).has_value(),
               "cannot change pointer declaration at an existing site") ||
      !require(relationCount() == 2 &&
                   scalar(database, "SELECT SUM(count) FROM relation "
                                    "WHERE kind=24") == 2,
               "pointer declaration replacement left stale counts") ||
      !require(execute(database, "DELETE FROM symbol WHERE id=" +
                                     std::to_string(packed(otherPointer))),
               "cannot remove pointer declaration") ||
      !require(siteCount() == 2 && relationCount() == 1,
               "pointer declaration deletion did not cascade"))
    return false;

  const std::array provenance{
      facts::storage::FactProvenance{1, "/project/input.cpp", "test"},
      facts::storage::FactProvenance{2, "/project/invocations.h", "test"}};
  if (!require(storage.begin().has_value(), "cannot start provenance write") ||
      !require(!storage.registerFactProvenance(std::span{provenance}.first(1)),
               "pointer-only file escaped provenance validation") ||
      !require(storage.registerFactProvenance(provenance).has_value(),
               "cannot register pointer site provenance") ||
      !require(storage.rollback().has_value(), "cannot roll back provenance"))
    return false;

  const std::array entries{facts::CallGraphEntry{caller, caller}};
  const std::array unresolved{facts::UnresolvedCallSite{
      caller, 1, {.line = 9, .column = 3, .offset = 90}}};
  if (!require(storage.addUnresolvedCallSites(unresolved).has_value(),
               "cannot seed legacy unresolved evidence") ||
      !require(!storage.addCallGraphFacts({}, {}, {}, entries, {},
                                         std::array{invalid}),
               "invalid regenerated pointer facts were accepted") ||
      !require(siteCount() == 2 && relationCount() == 1 &&
                   scalar(database, "SELECT COUNT(*) FROM "
                                    "callgraph_unresolved_site") == 1,
               "failed regeneration lost previous call evidence") ||
      !require(storage.addCallGraphFacts({}, {}, {}, entries, {},
                                         std::array{expression}).has_value(),
               "cannot regenerate pointer call facts") ||
      !require(siteCount() == 1 && relationCount() == 0 &&
                   scalar(database, "SELECT COUNT(*) FROM "
                                    "callgraph_unresolved_site") == 0,
               "regeneration did not replace old call evidence") ||
      !require(storage.clearCallGraphFacts(std::array{caller}).has_value(),
               "cannot clear pointer call graph facts") ||
      !require(siteCount() == 0 &&
                   scalar(database, "SELECT COUNT(*) FROM callgraph_entry") == 0,
               "cleared graph retained pointer evidence or cache entry"))
    return false;
  return require(scalar(database, "SELECT COUNT(*) FROM pragma_foreign_key_check") == 0,
                 "pointer evidence violates foreign keys");
}

bool verifyPointerMigration(sqlite3 *database, facts::SymbolId caller) {
  const auto key = std::to_string(packed(caller));
  if (!require(execute(database,
                       "DROP TABLE callgraph_pointer_call_site; "
                       "DROP TABLE callgraph_run_pointer_call_site; "
                       "INSERT INTO callgraph_entry VALUES(" + key + "," +
                           key + "); PRAGMA user_version=13;"),
               "cannot seed version-thirteen database"))
    return false;
  if (!require(facts::storage::migrateSchema(database).has_value(),
               "pointer call migration failed") ||
      !require(scalar(database, "PRAGMA user_version") == 14,
               "pointer call migration did not record version fourteen") ||
      !require(scalar(database, "SELECT COUNT(*) FROM callgraph_entry") == 0,
               "migration retained stale call graph entries") ||
      !require(scalar(database, "SELECT COUNT(*) FROM "
                               "callgraph_pointer_call_site") == 0,
               "migration invented pointer evidence") ||
      !require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'callgraph_run_pointer_call_site')") == 11,
               "migration omitted pointer run snapshots") ||
      !require(execute(database, "INSERT INTO callgraph_entry VALUES(" + key +
                                     "," + key + ")"),
               "cannot seed current call graph entry") ||
      !require(facts::storage::migrateSchema(database).has_value(),
               "repeated pointer call migration failed"))
    return false;
  return require(scalar(database, "SELECT COUNT(*) FROM callgraph_entry") == 1,
                 "idempotent migration invalidated current graph entries");
}

bool verifyPointerStaging(const std::filesystem::path &path) {
  facts::FactStore store{path.string()};
  facts::PointerCallSite site{};
  site.signature = "void (*)()";
  site.expression = "fp";
  if (!require(store.begin().has_value(), "cannot begin pointer staging"))
    return false;
  store.stagePointerCallSite(site);
  if (!require(store.takePointerCallSites().size() == 1 &&
                   store.pointerCallSites().empty(),
               "taking staged pointer invocations did not drain the queue"))
    return false;
  store.stagePointerCallSite(site);
  if (!require(store.rollback().has_value(), "cannot roll back pointer staging") ||
      !require(store.pointerCallSites().empty(),
               "rolled back pointer invocations survived transaction") ||
      !require(store.begin().has_value(), "cannot restart pointer staging"))
    return false;
  store.stagePointerCallSite(site);
  return require(store.end(false).has_value(), "cannot finish pointer staging") &&
         require(store.pointerCallSites().empty(),
                 "committed pointer staging was not cleared");
}

} // namespace

bool verifyPointerCallStorage(const std::filesystem::path &path) {
  removeDatabase(path);
  {
    facts::Storage storage{path.string()};
    facts::Function caller{};
    caller.id.file = 1;
    caller.usr = "c:@F@pointer_caller";
    caller.qualifiedName = "pointer_caller";
    facts::Variable pointer{};
    pointer.id.file = 1;
    pointer.usr = "c:@V@fp";
    pointer.qualifiedName = "fp";
    const auto savedCaller = storage.save(caller);
    const auto savedPointer = storage.save(pointer);
    pointer.usr = "c:@V@other_fp";
    pointer.qualifiedName = "other_fp";
    const auto otherPointer = storage.save(pointer);
    if (!require(savedCaller && savedPointer && otherPointer,
                 "cannot seed pointer call symbols"))
      return false;
    sqlite3 *database = nullptr;
    if (!require(sqlite3_open(path.c_str(), &database) == SQLITE_OK,
                 "cannot inspect pointer call database")) {
      sqlite3_close(database);
      return false;
    }
    const auto valid =
        execute(database, "PRAGMA foreign_keys=ON;") &&
        verifyPointerFacts(storage, database, *savedCaller, *savedPointer,
                           *otherPointer) &&
        verifyPointerMigration(database, *savedCaller);
    sqlite3_close(database);
    if (!valid)
      return false;
  }
  return verifyPointerStaging(path);
}

} // namespace storage_schema_test
