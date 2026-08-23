#include "storage/SqliteDatabase.h"

#include <sqlite3.h>

#include <climits>
#include <concepts>
#include <expected>
#include <iostream>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using facts::storage::bindParameters;
using facts::storage::BulkOptions;
using facts::storage::BulkResult;
using facts::storage::collect;
using facts::storage::CommandResult;
using facts::storage::Database;
using facts::storage::Row;
using facts::storage::Transaction;
using facts::storage::TransactionMode;
using facts::storage::TransactionState;

static_assert(std::same_as<
              decltype(std::declval<Database &>().execute(std::string_view{})),
              std::expected<void, std::error_code>>);
static_assert(std::movable<Transaction>);
static_assert(!std::copy_constructible<Transaction>);

bool require(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

std::expected<std::vector<int>, std::error_code> integers(Database &database,
                                                          std::string sql) {
  return collect(database.query(
      std::move(sql), [](const Row &row) { return row.get<int>(0); }));
}

bool equals(Database &database, std::string sql, std::vector<int> expected,
            std::string_view message) {
  const auto actual = integers(database, std::move(sql));
  return require(actual.has_value() && *actual == expected, message);
}

bool testCommands() {
  auto opened = Database::open(":memory:", Database::readWrite);
  if (!require(opened.has_value(), "command database open failed")) {
    return false;
  }
  Database &database = *opened;

  const auto schema = database.executeCommand(
      "CREATE TABLE command(id INTEGER PRIMARY KEY, value TEXT UNIQUE)");
  const auto inserted = database.executeCommand(
      "INSERT INTO command(value) VALUES ('one'), ('two')");
  const auto updated =
      database.executeCommand("UPDATE command SET value = value || '-updated'");
  const auto removed =
      database.executeCommand("DELETE FROM command WHERE id = 2");
  const auto compatible =
      database.execute("UPDATE command SET value = 'compat' WHERE id = 1");
  const auto secondSchema =
      database.executeCommand("CREATE TABLE command_log(value TEXT)");

  if (!require(schema == CommandResult{0}, "DDL change count was not zero") ||
      !require(inserted == CommandResult{2}, "INSERT change count was wrong") ||
      !require(updated == CommandResult{2}, "UPDATE change count was wrong") ||
      !require(removed == CommandResult{1}, "DELETE change count was wrong") ||
      !require(compatible.has_value(),
               "execute compatibility wrapper failed") ||
      !require(secondSchema == CommandResult{0},
               "DDL reused a previous DML change count")) {
    return false;
  }

  const auto returning = collect(database.query(
      "INSERT INTO command(value) VALUES ('returning') RETURNING id",
      [](const Row &row) { return row.get<int>(0); }));
  if (!require(returning.has_value() && returning->size() == 1,
               "INSERT RETURNING did not provide an identifier")) {
    return false;
  }

  const auto trailing = database.executeCommand(
      "INSERT INTO command(value) VALUES ('blocked'); DELETE FROM command");
  if (!require(!trailing.has_value(), "trailing SQL was accepted") ||
      !equals(database, "SELECT COUNT(*) FROM command WHERE value = 'blocked'",
              {0}, "the rejected first statement was executed")) {
    return false;
  }

  const auto script = database.executeScript(
      "INSERT INTO command(value) VALUES ('script-one');"
      "INSERT INTO command(value) VALUES ('script-two');");
  const auto failedScript = database.executeScript(
      "INSERT INTO command(value) VALUES ('before-error');"
      "INSERT INTO command(value) VALUES ('before-error');"
      "INSERT INTO command(value) VALUES ('after-error');");
  return require(script.has_value(), "multi-statement script failed") &&
         require(!failedScript.has_value(),
                 "script failure was not returned") &&
         equals(database,
                "SELECT COUNT(*) FROM command WHERE value = 'before-error'",
                {1}, "script did not stop at its first failure") &&
         equals(database,
                "SELECT COUNT(*) FROM command WHERE value = 'after-error'", {0},
                "script continued after its first failure") &&
         require(facts::storage::detail::widenLegacyChangeCount(INT_MAX) ==
                     static_cast<std::int64_t>(INT_MAX),
                 "legacy change count was not widened");
}

bool testRawTransactionWrappers() {
  sqlite3 *raw = nullptr;
  if (!require(sqlite3_open(":memory:", &raw) == SQLITE_OK,
               "raw SQLite open failed")) {
    return false;
  }
  bool ok = false;
  {
    auto read = Transaction::read(raw);
    auto write = read && read->rollback() ? Transaction::write(raw)
                                          : std::unexpected(read.error());
    ok = require(read.has_value(), "Transaction::read wrapper failed") &&
         require(write.has_value(), "Transaction::write wrapper failed") &&
         require(write && write->rollback().has_value(),
                 "raw write rollback failed");
  }
  sqlite3_close_v2(raw);
  return ok;
}

bool testTransactionModesAndStates(Database &database) {
  for (const auto mode : {TransactionMode::deferred, TransactionMode::immediate,
                          TransactionMode::exclusive}) {
    auto transaction = database.transaction(mode);
    if (!require(transaction.has_value(), "transaction mode failed") ||
        !require(transaction->state() == TransactionState::active,
                 "new transaction was not active") ||
        !require(transaction->rollback().has_value(), "rollback failed") ||
        !require(transaction->state() == TransactionState::rolledBack,
                 "rollback state was not recorded") ||
        !require(!transaction->commit().has_value(),
                 "second terminal operation succeeded")) {
      return false;
    }
  }
  return true;
}

bool testTransactions() {
  auto opened = Database::open(":memory:", Database::readWrite);
  if (!require(opened.has_value(), "transaction database open failed") ||
      !require(
          opened
              ->executeScript("PRAGMA foreign_keys=ON;"
                              "CREATE TABLE parent(id INTEGER PRIMARY KEY);"
                              "CREATE TABLE child(parent_id INTEGER,"
                              " FOREIGN KEY(parent_id) REFERENCES parent(id)"
                              " DEFERRABLE INITIALLY DEFERRED);"
                              "CREATE TABLE tx(value INTEGER);")
              .has_value(),
          "transaction schema failed")) {
    return false;
  }
  Database &database = *opened;

  if (!testTransactionModesAndStates(database)) {
    return false;
  }

  {
    auto transaction = database.write();
    if (!require(transaction.has_value(), "write wrapper failed") ||
        !require(database.execute("INSERT INTO tx VALUES (1)").has_value(),
                 "transaction insert failed")) {
      return false;
    }
  }
  if (!equals(database, "SELECT COUNT(*) FROM tx", {0},
              "destructor did not roll back")) {
    return false;
  }

  auto committed = database.read();
  if (!require(committed.has_value(), "read wrapper failed") ||
      !require(committed->commit().has_value(), "commit failed") ||
      !require(committed->state() == TransactionState::committed,
               "commit state was not recorded") ||
      !require(!committed->rollback().has_value(),
               "rollback after commit succeeded")) {
    return false;
  }

  auto retryable = database.transaction(TransactionMode::deferred);
  if (!require(retryable.has_value(), "deferred transaction failed") ||
      !require(database.execute("INSERT INTO child VALUES (42)").has_value(),
               "deferred constraint insert failed") ||
      !require(!retryable->commit().has_value(),
               "deferred constraint commit unexpectedly succeeded") ||
      !require(retryable->active(), "failed commit was not retryable") ||
      !require(database.execute("INSERT INTO parent VALUES (42)").has_value(),
               "constraint repair failed") ||
      !require(retryable->commit().has_value(), "retry commit failed")) {
    return false;
  }

  auto outer = database.write();
  const auto nested = database.write();
  if (!require(outer.has_value(), "outer transaction failed") ||
      !require(!nested.has_value(), "nested BEGIN was accepted") ||
      !require(outer->active(), "nested BEGIN changed outer state") ||
      !require(outer->rollback().has_value(), "outer rollback failed")) {
    return false;
  }

  std::optional<Transaction> lifetime;
  {
    auto temporary = Database::open(":memory:", Database::readWrite);
    if (!require(temporary.has_value(), "lifetime database open failed") ||
        !require(
            temporary->execute("CREATE TABLE alive(value INTEGER)").has_value(),
            "lifetime schema failed")) {
      return false;
    }
    auto transaction = temporary->write();
    if (!require(transaction.has_value(), "lifetime transaction failed") ||
        !require(temporary->execute("INSERT INTO alive VALUES (1)").has_value(),
                 "lifetime insert failed")) {
      return false;
    }
    lifetime.emplace(std::move(*transaction));
  }
  return require(lifetime->commit().has_value(),
                 "transaction did not retain the shared connection") &&
         testRawTransactionWrappers();
}

struct BulkValue {
  int id;
  std::string value;
};

struct CopyTracked {
  static inline int copies = 0;
  int id;

  explicit CopyTracked(int value) : id(value) {}

  CopyTracked(const CopyTracked &other) : id(other.id) { ++copies; }

  CopyTracked(CopyTracked &&) noexcept = default;
  CopyTracked &operator=(const CopyTracked &) = delete;
  CopyTracked &operator=(CopyTracked &&) = default;
};

facts::Generator<int> singlePassIds() {
  co_yield 6;
  co_yield 7;
}

bool testSuccessfulBulk(Database &database) {
  std::vector<BulkValue> values{{1, "one"}, {2, "two"}, {3, "three"}};
  sqlite3_stmt *prepared = nullptr;
  bool preparedOnce = true;
  const auto inserted = database.executeBulk(
      "INSERT INTO bulk(id, value) VALUES (?1, ?2)", values,
      [&](sqlite3_stmt *statement, const BulkValue &value) {
        preparedOnce = preparedOnce && (!prepared || prepared == statement);
        prepared = statement;
        return bindParameters(statement, value.id, value.value);
      });
  const auto updated = database.executeBulk(
      "UPDATE bulk SET value = value || '-updated' WHERE id = ?1", values,
      [](sqlite3_stmt *statement, const BulkValue &value) {
        return bindParameters(statement, value.id);
      });
  const std::vector<int> removedIds{2, 3};
  const auto removed =
      database.executeBulk("DELETE FROM bulk WHERE id = ?1", removedIds,
                           [](sqlite3_stmt *statement, int id) {
                             return bindParameters(statement, id);
                           });

  CopyTracked::copies = 0;
  std::vector<CopyTracked> piped;
  piped.emplace_back(4);
  piped.emplace_back(5);
  const auto pipedResult =
      piped |
      database.bulk("INSERT INTO bulk(id, value) VALUES (?1, 'pipe')",
                    [](sqlite3_stmt *statement, const CopyTracked &value) {
                      return bindParameters(statement, value.id);
                    });
  const auto singlePass =
      singlePassIds() |
      database.bulk("INSERT INTO bulk(id, value) VALUES (?1, 'single')",
                    [](sqlite3_stmt *statement, int id) {
                      return bindParameters(statement, id);
                    });

  return require(inserted == BulkResult{3, 3},
                 "bulk INSERT totals were wrong") &&
         require(preparedOnce, "bulk statement was not prepared once") &&
         require(updated == BulkResult{3, 3},
                 "bulk UPDATE totals were wrong") &&
         require(removed == BulkResult{2, 2},
                 "bulk DELETE totals were wrong") &&
         require(pipedResult == BulkResult{2, 2},
                 "pipe bulk result was wrong") &&
         require(CopyTracked::copies == 0, "pipe adaptor copied its input") &&
         require(singlePass == BulkResult{2, 2},
                 "single-pass bulk input failed");
}

bool testBulkFailures(Database &database) {
  const std::vector<int> duplicate{10, 10};
  const auto stepped =
      database.executeBulk("INSERT INTO bulk(id, value) VALUES (?1, 'step')",
                           duplicate, [](sqlite3_stmt *statement, int id) {
                             return bindParameters(statement, id);
                           });
  if (!require(!stepped.has_value(), "middle step failure was accepted") ||
      !equals(database, "SELECT COUNT(*) FROM bulk WHERE id = 10", {0},
              "atomic step failure did not roll back")) {
    return false;
  }

  const std::vector<int> bindValues{20, 21, 22};
  const auto bound =
      database.executeBulk("INSERT INTO bulk(id, value) VALUES (?1, 'bind')",
                           bindValues, [](sqlite3_stmt *statement, int id) {
                             return id != 21 && bindParameters(statement, id);
                           });
  if (!require(!bound.has_value(), "middle bind failure was accepted") ||
      !equals(database, "SELECT COUNT(*) FROM bulk WHERE id BETWEEN 20 AND 22",
              {0}, "atomic bind failure did not roll back")) {
    return false;
  }

  auto outer = database.write();
  if (!require(outer.has_value(), "savepoint outer transaction failed") ||
      !require(
          database.execute("INSERT INTO bulk(id, value) VALUES (100, 'outer')")
              .has_value(),
          "outer transaction insert failed")) {
    return false;
  }
  const std::vector<int> nestedDuplicate{30, 30};
  const auto nested = database.executeBulk(
      "INSERT INTO bulk(id, value) VALUES (?1, 'nested')", nestedDuplicate,
      [](sqlite3_stmt *statement, int id) {
        return bindParameters(statement, id);
      });
  if (!require(!nested.has_value(), "nested step failure was accepted") ||
      !equals(database, "SELECT COUNT(*) FROM bulk WHERE id = 30", {0},
              "savepoint did not roll back its batch") ||
      !equals(database, "SELECT COUNT(*) FROM bulk WHERE id = 100", {1},
              "savepoint rolled back the outer transaction") ||
      !require(outer->commit().has_value(), "outer commit failed")) {
    return false;
  }

  auto managed = database.write();
  const std::vector<int> unmanagedDuplicate{40, 40};
  const auto unmanaged = database.executeBulk(
      "INSERT INTO bulk(id, value) VALUES (?1, 'managed')", unmanagedDuplicate,
      [](sqlite3_stmt *statement, int id) {
        return bindParameters(statement, id);
      },
      BulkOptions{.atomic = false});
  return require(managed.has_value(), "caller-managed transaction failed") &&
         require(!unmanaged.has_value(),
                 "caller-managed middle failure was accepted") &&
         equals(database, "SELECT COUNT(*) FROM bulk WHERE id = 40", {1},
                "atomic=false performed transaction control") &&
         require(managed->rollback().has_value(),
                 "caller-managed rollback failed") &&
         equals(database, "SELECT COUNT(*) FROM bulk WHERE id = 40", {0},
                "caller-managed rollback did not own the decision");
}

bool testBulk() {
  auto opened = Database::open(":memory:", Database::readWrite);
  return require(opened.has_value(), "bulk database open failed") &&
         require(
             opened
                 ->execute(
                     "CREATE TABLE bulk(id INTEGER PRIMARY KEY, value TEXT)")
                 .has_value(),
             "bulk schema failed") &&
         testSuccessfulBulk(*opened) && testBulkFailures(*opened);
}

} // namespace

int main() {
  return testCommands() && testTransactions() && testBulk() ? 0 : 1;
}
