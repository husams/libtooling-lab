#include "storage/FactStore.h"

#include <sqlite3.h>

#include <array>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <ranges>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <variant>

namespace {

int scalar(sqlite3 *database, const char *sql) {
  sqlite3_stmt *statement = nullptr;
  assert(sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) ==
         SQLITE_OK);
  assert(sqlite3_step(statement) == SQLITE_ROW);
  const int value = sqlite3_column_int(statement, 0);
  sqlite3_finalize(statement);
  return value;
}

template <typename Model>
Model makeSymbol(std::string usr, std::string qualifiedName) {
  Model symbol;
  symbol.usr = std::move(usr);
  symbol.qualifiedName = std::move(qualifiedName);
  return symbol;
}

bool addSharedSymbol(const std::filesystem::path &databasePath, std::size_t) {
  facts::FactStore store(databasePath.string());
  const auto id =
      store.save(7, makeSymbol<facts::Function>("c:@S@Shared", "Shared"));
  return id && *id == facts::SymbolId{7, 0};
}

bool addSharedUsrAcrossFiles(const std::filesystem::path &databasePath,
                             std::size_t childIndex) {
  facts::FactStore store(databasePath.string());
  const auto saved =
      store.save(static_cast<facts::FileId>(20 + childIndex),
                 makeSymbol<facts::Record>("c:@S@CrossFile", "CrossFile"));
  const auto found = store.findId("c:@S@CrossFile");
  return saved && found && *found && *saved == **found;
}

bool addDistinctSymbol(const std::filesystem::path &databasePath,
                       std::size_t childIndex) {
  facts::FactStore store(databasePath.string());
  const auto suffix = std::to_string(childIndex);
  const auto id =
      store.save(7, makeSymbol<facts::Symbol>("c:@S@Concurrent" + suffix,
                                              "Concurrent" + suffix));
  return id && id->file == 7 && id->index > 0;
}

using Writer = bool (*)(const std::filesystem::path &, std::size_t);

void verifyUseSiteAggregation(const std::filesystem::path &databasePath,
                              bool reverseOrder) {
  std::filesystem::remove(databasePath);
  facts::SymbolId owner;
  facts::SymbolId target;
  {
    facts::FactStore seed(databasePath.string());
    const auto savedOwner =
        seed.save(30, makeSymbol<facts::Function>("c:@F@owner", "owner"));
    const auto savedTarget =
        seed.save(31, makeSymbol<facts::Variable>("c:@V@target", "target"));
    assert(savedOwner && savedTarget);
    owner = *savedOwner;
    target = *savedTarget;
  }

  const std::array relation{facts::Relation{
      .source = owner,
      .destination = target,
      .kind = facts::RelationKind::Uses,
  }};
  const std::array firstSites{
      facts::RelationSite{
          .source = owner,
          .destination = target,
          .file = 31,
          .location = {.line = 4, .column = 5, .offset = 40},
      },
      facts::RelationSite{
          .source = owner,
          .destination = target,
          .file = 31,
          .location = {.line = 5, .column = 6, .offset = 50},
      },
  };
  const std::array secondSites{
      firstSites.back(),
      facts::RelationSite{
          .source = owner,
          .destination = target,
          .file = 32,
          .location = {.line = 6, .column = 7, .offset = 60},
      },
  };

  const auto write = [&](std::span<const facts::RelationSite> sites) {
    facts::FactStore store(databasePath.string());
    assert(store.addUseFacts(relation, sites));
  };
  if (reverseOrder) {
    write(secondSites);
    write(firstSites);
  } else {
    write(firstSites);
    write(secondSites);
  }

  {
    facts::FactStore store(databasePath.string());
    const std::array invalidRelation{facts::Relation{
        .source = owner,
        .destination = {.file = 99, .index = 1},
        .kind = facts::RelationKind::Uses,
    }};
    const std::array invalidSite{facts::RelationSite{
        .source = owner,
        .destination = invalidRelation.front().destination,
        .file = 31,
        .location = {.line = 9, .column = 1, .offset = 90},
    }};
    assert(!store.addUseFacts(invalidRelation, invalidSite));
  }

  sqlite3 *database = nullptr;
  assert(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK);
  assert(scalar(database, "SELECT COUNT(*) FROM relation") == 1);
  assert(scalar(database, "SELECT count FROM relation") == 3);
  assert(scalar(database, "SELECT COUNT(*) FROM relation_site") == 3);
  assert(scalar(database,
                "SELECT COUNT(*) FROM relation_site WHERE "
                "(file_id=31 AND line=4 AND col=5 AND offset=40) OR "
                "(file_id=31 AND line=5 AND col=6 AND offset=50) OR "
                "(file_id=32 AND line=6 AND col=7 AND offset=60)") == 3);
  sqlite3_close(database);
  std::filesystem::remove(databasePath);
}

void runConcurrentWriters(const std::filesystem::path &databasePath,
                          Writer writer) {
  std::array<pid_t, 8> children{};
  int startPipe[2]{};
  assert(pipe(startPipe) == 0);

  for (const auto childIndex :
       std::views::iota(std::size_t{0}, children.size())) {
    auto &child = children[childIndex];
    child = fork();
    assert(child >= 0);
    if (child == 0) {
      close(startPipe[1]);
      char signal = 0;
      const auto ignored = read(startPipe[0], &signal, 1);
      static_cast<void>(ignored);
      close(startPipe[0]);
      _exit(writer(databasePath, childIndex) ? EXIT_SUCCESS : EXIT_FAILURE);
    }
  }

  close(startPipe[0]);
  close(startPipe[1]);
  for (const auto child : children) {
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == EXIT_SUCCESS);
  }
}

} // namespace

int main(int argc, char **argv) {
  assert(argc == 2);
  const std::filesystem::path databasePath = argv[1];
  std::filesystem::remove(databasePath);

  runConcurrentWriters(databasePath, addSharedSymbol);
  runConcurrentWriters(databasePath, addDistinctSymbol);

  const auto crossFileDatabasePath = databasePath.string() + ".cross-file";
  std::filesystem::remove(crossFileDatabasePath);
  runConcurrentWriters(crossFileDatabasePath, addSharedUsrAcrossFiles);
  {
    sqlite3 *crossFileDatabase = nullptr;
    assert(sqlite3_open(crossFileDatabasePath.c_str(), &crossFileDatabase) ==
           SQLITE_OK);
    assert(scalar(crossFileDatabase, "SELECT COUNT(*) FROM symbol") == 1);
    assert(scalar(crossFileDatabase,
                  "SELECT COUNT(DISTINCT usr) FROM symbol") == 1);
    sqlite3_close(crossFileDatabase);
  }
  std::filesystem::remove(crossFileDatabasePath);

  facts::FactStore first(databasePath.string());
  assert(first.cachedIdCount() == 0);

  auto shared = makeSymbol<facts::Function>("c:@S@Shared", "Shared");
  assert(shared.id == facts::SymbolId{});
  shared.definition = facts::Region{20, 30};
  shared.parameters.push_back(facts::Parameter{
      .name = "value",
      .type = {0, 1},
      .loc = {3, 4, 5},
      .region = {6, 7},
      .hasDefault = true,
      .defaultValue =
          facts::Initializer{
              .expression = "2 + 3",
              .evaluated =
                  facts::EvaluatedValue{
                      .kind = facts::EvaluatedValueKind::Integer,
                      .value = "5",
                  },
          },
  });
  const auto firstSymbol = first.save(7, shared);
  constexpr facts::SymbolId expectedShared{7, 0};
  assert(firstSymbol && *firstSymbol == expectedShared);
  assert(first.cachedIdCount() == 1);
  assert(first.contains(shared.usr));

  const auto loadedShared = first.load<facts::Function>(*firstSymbol);
  assert(loadedShared);
  assert(loadedShared->usr == shared.usr);
  assert(loadedShared->qualifiedName == shared.qualifiedName);
  assert(loadedShared->definition);
  assert(loadedShared->definition->offset == 20);
  assert(loadedShared->definition->size == 30);
  assert(loadedShared->parameters.size() == 1);
  assert(loadedShared->parameters.front().name == "value");
  constexpr facts::SymbolId expectedParameterType{0, 1};
  assert(loadedShared->parameters.front().type == expectedParameterType);
  assert(loadedShared->parameters.front().hasDefault);
  assert(loadedShared->parameters.front().defaultValue);
  assert(loadedShared->parameters.front().defaultValue->expression == "2 + 3");
  assert(loadedShared->parameters.front().defaultValue->evaluated);
  assert(loadedShared->parameters.front().defaultValue->evaluated->value ==
         "5");

  facts::AnySymbol anySymbol{*loadedShared};
  const auto variantSymbol = first.save(std::move(anySymbol));
  assert(variantSymbol == firstSymbol);
  assert(first.cachedIdCount() == 1);

  const auto missing = first.load<facts::Function>({7, 9999});
  assert(!missing);
  assert(first.cachedIdCount() == 1);

  auto other = makeSymbol<facts::Variable>("c:@V@other", "other");
  assert(other.id == facts::SymbolId{});
  other.flags |= facts::bit(facts::ExternStorageBit);
  const auto otherSymbol = first.save(9, other);
  assert(otherSymbol && (*otherSymbol == facts::SymbolId{9, 0}));
  assert(first.cachedIdCount() == 2);

  auto otherDefinition = makeSymbol<facts::Variable>("c:@V@other", "other");
  otherDefinition.flags |= facts::bit(facts::DefinitionBit);
  otherDefinition.definition = facts::Region{40, 5};
  otherDefinition.initializer = facts::Initializer{
      .expression = "\"stored\"",
      .evaluated =
          facts::EvaluatedValue{
              .kind = facts::EvaluatedValueKind::String,
              .value = "stored",
          },
  };
  assert(first.save(10, std::move(otherDefinition)) == otherSymbol);
  assert(first.save(9, std::move(other)) == otherSymbol);
  assert(first.cachedIdCount() == 2);

  auto sharedDeclaration = makeSymbol<facts::Function>("c:@S@Shared", "Shared");
  sharedDeclaration.parameters.push_back(facts::Parameter{
      .name = "value",
      .type = {0, 1},
      .loc = {3, 4, 5},
      .region = {6, 7},
  });
  const auto sharedDeclarationId = first.save(9, std::move(sharedDeclaration));
  assert(sharedDeclarationId == firstSymbol);
  assert(first.cachedIdCount() == 2);

  const auto mergedShared = first.load<facts::Function>(*firstSymbol);
  assert(mergedShared && mergedShared->parameters.size() == 1);
  assert(mergedShared->parameters.front().hasDefault);
  assert(mergedShared->parameters.front().defaultValue);
  assert(mergedShared->parameters.front().defaultValue->expression == "2 + 3");

  const auto updatedSymbol = first.save(7, std::move(shared));
  assert(updatedSymbol == firstSymbol);
  assert(first.cachedIdCount() == 2);

  facts::FactStore second(databasePath.string());
  assert(second.cachedIdCount() == 0);
  const auto emptyUsr = second.load<facts::Function>("");
  assert(!emptyUsr);
  assert(second.cachedIdCount() == 0);

  const auto missingUsr = second.load<facts::Function>("c:@missing");
  assert(missingUsr && !*missingUsr);
  assert(second.cachedIdCount() == 0);
  assert(!second.contains("c:@missing"));

  const auto sharedId = second.findId("c:@S@Shared");
  assert(sharedId && *sharedId && **sharedId == *firstSymbol);
  assert(second.contains("c:@S@Shared"));
  assert(second.cachedIdCount() == 1);
  assert(second.findId("c:@S@Shared") == sharedId);

  const auto sharedSymbol = second.load<facts::Function>("c:@S@Shared");
  assert(sharedSymbol && *sharedSymbol);
  assert(second.cachedIdCount() == 1);
  assert((*sharedSymbol)->id == *firstSymbol);

  auto loadedOther = second.load<facts::Variable>(*otherSymbol);
  assert(loadedOther);
  assert(loadedOther->definition);
  assert(loadedOther->definitionFile == 10);
  assert(loadedOther->definition->offset == 40);
  assert(loadedOther->initializer);
  assert(loadedOther->initializer->expression == "\"stored\"");
  assert(loadedOther->initializer->evaluated);
  assert(loadedOther->initializer->evaluated->kind ==
         facts::EvaluatedValueKind::String);
  assert(loadedOther->initializer->evaluated->value == "stored");
  assert((loadedOther->flags & facts::bit(facts::DefinitionBit)) != 0);
  assert((loadedOther->flags & facts::bit(facts::ExternStorageBit)) != 0);
  assert(second.cachedIdCount() == 2);
  const auto sameOther = second.save(std::move(*loadedOther));
  assert(sameOther == otherSymbol);
  assert(second.cachedIdCount() == 2);

  const auto unresolvedType = first.findId("c:@N@namespace");
  assert(unresolvedType && !*unresolvedType);
  assert(!first.contains("c:@N@namespace"));

  const auto roundTrip = [&]<typename Model>(std::string usr) {
    auto model = makeSymbol<Model>(usr, usr);
    const auto id = first.save(9, std::move(model));
    assert(id);
    const auto resolved = first.findId(usr);
    assert(resolved && *resolved && **resolved == *id);
    assert(first.load<Model>(*id));
    const auto loaded = first.load<Model>(usr);
    assert(loaded && *loaded && (*loaded)->id == *id);
  };
  roundTrip.template operator()<facts::Record>("c:@S@Record");
  roundTrip.template operator()<facts::Enumeration>("c:@E@Enumeration");
  roundTrip.template operator()<facts::Symbol>("c:@N@namespace");
  roundTrip.template operator()<facts::Symbol>("c:@M@other");

  sqlite3 *database = nullptr;
  assert(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK);
  assert(scalar(database, "SELECT COUNT(*) FROM symbol") == 14);
  assert(scalar(database, "SELECT COUNT(DISTINCT file_id) FROM symbol") == 2);
  assert(
      scalar(database,
             "SELECT COUNT(DISTINCT file_index) FROM symbol WHERE file_id=7") ==
      9);
  assert(scalar(database, "SELECT COUNT(*) FROM definition") == 2);
  assert(scalar(database, "SELECT COUNT(*) FROM definition WHERE file_id=7 AND "
                          "offset=20 AND size=30") == 1);
  assert(scalar(database,
                "SELECT COUNT(*) FROM definition WHERE file_id=10 AND "
                "offset=40 AND size=5") == 1);
  assert(scalar(database, "SELECT COUNT(*) FROM parameter") == 1);
  assert(scalar(database, "SELECT has_default FROM parameter") == 1);
  assert(scalar(database, "SELECT COUNT(*) FROM variable_initializer WHERE "
                          "expression='\"stored\"' AND "
                          "evaluated_kind='string' AND "
                          "evaluated_value='stored'") == 1);
  sqlite3_close(database);

  const auto cacheDatabasePath = databasePath.string() + ".cache";
  std::filesystem::remove(cacheDatabasePath);
  facts::SymbolId cachedId;
  {
    facts::FactStore seed(cacheDatabasePath);
    const auto saved =
        seed.save(11, makeSymbol<facts::Symbol>("c:@S@Cached", "Cached"));
    assert(saved);
    cachedId = *saved;
  }
  facts::FactStore cached(cacheDatabasePath);
  const auto foundCachedId = cached.findId("c:@S@Cached");
  assert(foundCachedId && *foundCachedId && **foundCachedId == cachedId);
  assert(cached.load<facts::Symbol>(cachedId));

  sqlite3 *cacheDatabase = nullptr;
  assert(sqlite3_open(cacheDatabasePath.c_str(), &cacheDatabase) == SQLITE_OK);
  assert(sqlite3_exec(cacheDatabase, "DELETE FROM symbol", nullptr, nullptr,
                      nullptr) == SQLITE_OK);
  sqlite3_close(cacheDatabase);

  assert(cached.findId("c:@S@Cached") == foundCachedId);
  assert(!cached.load<facts::Symbol>(cachedId));
  std::filesystem::remove(cacheDatabasePath);

  const auto transactionDatabasePath = databasePath.string() + ".transaction";
  std::filesystem::remove(transactionDatabasePath);
  {
    facts::FactStore rolledBack(transactionDatabasePath);
    assert(rolledBack.begin());
    assert(rolledBack.save(
        12, makeSymbol<facts::Symbol>("c:@S@RolledBack", "RolledBack")));
    assert(rolledBack.cachedIdCount() == 1);
    assert(rolledBack.rollback());
    assert(rolledBack.cachedIdCount() == 0);
  }
  {
    facts::FactStore afterRollback(transactionDatabasePath);
    const auto missingAfterRollback =
        afterRollback.load<facts::Symbol>("c:@S@RolledBack");
    assert(missingAfterRollback && !*missingAfterRollback);
  }
  {
    facts::FactStore committed(transactionDatabasePath);
    assert(committed.begin());
    assert(committed.save(
        12, makeSymbol<facts::Symbol>("c:@S@Committed", "Committed")));
    assert(committed.end());
  }
  {
    facts::FactStore afterCommit(transactionDatabasePath);
    const auto loadedAfterCommit =
        afterCommit.load<facts::Symbol>("c:@S@Committed");
    assert(loadedAfterCommit && *loadedAfterCommit);
  }
  std::filesystem::remove(transactionDatabasePath);

  verifyUseSiteAggregation(databasePath.string() + ".uses-forward", false);
  verifyUseSiteAggregation(databasePath.string() + ".uses-reverse", true);
}
