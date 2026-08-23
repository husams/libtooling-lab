#include "storage/SqliteQuery.h"
#include "storage/SqliteDatabase.h"

#include <sqlite3.h>

#include <chrono>
#include <concepts>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

using facts::storage::Blob;
using facts::storage::collect;
using facts::storage::Database;
using facts::storage::QueryError;
using facts::storage::Row;

static_assert(std::ranges::input_range<facts::Generator<int>>);
static_assert(std::movable<Database>);
static_assert(!std::copy_constructible<Database>);

bool require(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

enum class Kind { record = 3 };

struct CustomBind {
  int value;
};

bool bindValue(sqlite3_stmt *statement, int position, const CustomBind &value) {
  return sqlite3_bind_int(statement, position, value.value) == SQLITE_OK;
}

struct BoundValues {
  bool flag;
  int integer;
  Kind kind;
  double real;
  std::string text;
  std::optional<std::string> missing;
  std::optional<int> optional;
  Blob blob;
  int custom;

  bool operator==(const BoundValues &) const = default;
};

bool testOpenOptions(const std::filesystem::path &path) {
  std::filesystem::remove(path);
  if (!require(!Database::open(path.string()).has_value(),
               "read-only open unexpectedly created a database")) {
    return false;
  }

  {
    auto opened = Database::open(path.string(), Database::readWrite);
    if (!require(opened.has_value(), "read-write open failed")) {
      return false;
    }
    if (!require(
            opened->execute("CREATE TABLE item(value INTEGER)").has_value(),
            "schema creation failed")) {
      return false;
    }
  }

  auto opened = Database::open(path.string());
  if (!require(opened.has_value(), "read-only reopen failed")) {
    return false;
  }
  return require(!opened->execute("INSERT INTO item VALUES (1)").has_value(),
                 "read-only connection accepted a write");
}

bool testBindingsAndRows(Database &database) {
  std::string backing = "owned text";
  std::optional<std::string_view> optionalText = std::string_view{backing};
  Blob payload{std::byte{0x01}, std::byte{0x7f}};
  auto values = collect(database.query(
      "SELECT ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9",
      [](const Row &row) {
        return BoundValues{
            row.get<bool>(0),
            row.get<int>(1),
            row.get<Kind>(2),
            row.get<double>(3),
            row.get<std::string>(4),
            row.get<std::optional<std::string>>(5),
            row.get<std::optional<int>>(6),
            row.get<Blob>(7),
            row.get<int>(8),
        };
      },
      true, 42, Kind::record, 2.5, optionalText, nullptr, std::optional<int>{7},
      payload, CustomBind{11}));

  const BoundValues expected{true,         42, Kind::record, 2.5, "owned text",
                             std::nullopt, 7,  payload,      11};
  return require(values.has_value(), "binding query failed") &&
         require(values->size() == 1 && values->front() == expected,
                 "binding query returned unexpected values");
}

bool testOwnedArguments(Database &database) {
  facts::Generator<std::string> textStream;
  {
    std::string local = "survives";
    std::optional<std::string_view> value{local};
    textStream = database.query(
        "SELECT ?1", [](const Row &row) { return row.get<std::string>(0); },
        value);
  }
  auto text = collect(std::move(textStream));

  facts::Generator<Blob> blobStream;
  {
    Blob local{std::byte{0x11}, std::byte{0x22}};
    blobStream = database.query(
        "SELECT ?1", [](const Row &row) { return row.get<Blob>(0); },
        std::span<std::byte>{local});
  }
  auto blob = collect(std::move(blobStream));

  return require(text.has_value() &&
                     *text == std::vector<std::string>{"survives"},
                 "lazy query did not own its text bind argument") &&
         require(blob.has_value() &&
                     *blob == std::vector<Blob>{Blob{std::byte{0x11},
                                                     std::byte{0x22}}},
                 "lazy query did not own its byte-range bind argument");
}

bool testRows(Database &database) {
  auto rows = database.rows("SELECT 1, 2.5, 'first', x'017f' UNION ALL "
                            "SELECT 2, 3.5, 'second', x'0210'");
  auto iterator = rows.begin();
  if (!require(iterator != rows.end(), "rows returned no first row")) {
    return false;
  }
  const bool first =
      require(iterator->integer(0) == 1, "row integer accessor failed") &&
      require(iterator->real(1) == 2.5, "row real accessor failed") &&
      require(iterator->text(2) == "first", "row text accessor failed") &&
      require(iterator->string(2) == "first", "row string accessor failed") &&
      require(iterator->blob(3) == Blob{std::byte{0x01}, std::byte{0x7f}},
              "row blob accessor failed");

  ++iterator;
  const bool second =
      require(iterator != rows.end(), "rows returned no second row") &&
      require(iterator->integer(0) == 2, "rows did not step to second row") &&
      require(iterator->text(2) == "second", "second row text accessor failed");
  ++iterator;
  return first && second &&
         require(iterator == rows.end(), "rows did not finish after two rows");
}

bool testViewsAndStepping(Database &database) {
  auto pipeline =
      database.query("SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3",
                     [](const Row &row) { return row.get<int>(0); }) |
      std::views::filter([](int value) { return value > 1; }) |
      std::views::transform([](int value) { return value * 10; }) |
      std::views::take(1);
  auto values = collect(std::move(pipeline));
  return require(values.has_value() && *values == std::vector{20},
                 "input range did not compose with standard views");
}

bool testErrors(Database &database) {
  bool lazyConstruction = true;
  try {
    auto unstarted = database.query("not SQL", [](const Row &) { return 0; });
    (void)unstarted;
  } catch (...) {
    lazyConstruction = false;
  }

  bool directError = false;
  auto invalid = database.query("not SQL", [](const Row &) { return 0; });
  try {
    for ([[maybe_unused]] int value : invalid) {
    }
  } catch (const QueryError &) {
    directError = true;
  }

  auto collectedSyntaxError =
      collect(database.query("not SQL", [](const Row &) { return 0; }));
  auto decodeError = collect(database.query(
      "SELECT 'text'", [](const Row &row) { return row.get<int>(0); }));
  auto arityError = collect(database.query(
      "SELECT ?2", [](const Row &row) { return row.get<int>(0); }, 1));
  auto rangeError = collect(database.query(
      "SELECT 1", [](const Row &row) { return row.get<int>(1); }));
  auto nullError = collect(database.query(
      "SELECT NULL", [](const Row &row) { return row.get<int>(0); }));

  bool mapperError = false;
  try {
    auto ignored = collect(database.query("SELECT 1", [](const Row &) -> int {
      throw std::runtime_error("mapper failed");
    }));
    (void)ignored;
  } catch (const std::runtime_error &) {
    mapperError = true;
  }

  return require(lazyConstruction, "query prepared before iteration") &&
         require(directError, "direct iteration did not throw QueryError") &&
         require(!collectedSyntaxError.has_value(),
                 "collect did not convert QueryError") &&
         require(!decodeError.has_value(),
                 "typed row mismatch did not become QueryError") &&
         require(!arityError.has_value(),
                 "bind arity mismatch did not become QueryError") &&
         require(!rangeError.has_value(),
                 "out-of-range column did not become QueryError") &&
         require(!nullError.has_value(),
                 "unexpected null did not become QueryError") &&
         require(mapperError, "collect swallowed a mapper exception");
}

bool testConnectionLifetime() {
  facts::Generator<int> stream;
  {
    auto opened = Database::open(":memory:", Database::readWrite);
    if (!require(opened.has_value(), "in-memory open failed")) {
      return false;
    }
    stream = opened->query("SELECT 7 UNION ALL SELECT 9",
                           [](const Row &row) { return row.get<int>(0); });
  }
  auto values = collect(std::move(stream));
  return require(values.has_value() && *values == std::vector{7, 9},
                 "lazy range did not retain its connection");
}

bool testExplicitMutationsAndTransactions(Database &database) {
  if (!require(database
                   .execute("CREATE TABLE mutation(id INTEGER PRIMARY KEY, "
                            "value TEXT)")
                   .has_value(),
               "mutation schema creation failed")) {
    return false;
  }

  auto transaction = database.write();
  if (!require(transaction.has_value(), "write transaction failed") ||
      !require(database.execute("INSERT INTO mutation VALUES (1, 'before')")
                   .has_value(),
               "insert failed") ||
      !require(database.execute("UPDATE mutation SET value='after' WHERE id=1")
                   .has_value(),
               "update failed") ||
      !require(transaction->commit().has_value(),
               "transaction commit failed")) {
    return false;
  }

  if (!require(database.execute("DELETE FROM mutation WHERE id=1").has_value(),
               "delete failed")) {
    return false;
  }
  auto count =
      collect(database.query("SELECT COUNT(*) FROM mutation",
                             [](const Row &row) { return row.get<int>(0); }));
  return require(count.has_value() && *count == std::vector{0},
                 "explicit mutations returned an unexpected result");
}

} // namespace

int main() {
  const auto unique =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto path =
      std::filesystem::temp_directory_path() /
      ("facts-sqlite-query-" + std::to_string(unique) + ".sqlite");

  const bool options = testOpenOptions(path);
  std::filesystem::remove(path);

  auto opened = Database::open(":memory:", Database::readWrite);
  if (!require(opened.has_value(), "shared test database open failed")) {
    return 1;
  }
  const bool queries = testBindingsAndRows(*opened) &&
                       testOwnedArguments(*opened) && testRows(*opened) &&
                       testViewsAndStepping(*opened) && testErrors(*opened) &&
                       testExplicitMutationsAndTransactions(*opened) &&
                       testConnectionLifetime();
  return options && queries ? 0 : 1;
}
