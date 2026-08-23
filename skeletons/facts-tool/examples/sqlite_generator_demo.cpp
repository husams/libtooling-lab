// sqlite_generator_demo — the coroutine query API against a file registry.
//
// The registry is the SQLite database facts-tool writes with --files-out: one
// `file(id, path)` row per translation unit and header it saw.
//
// The CMake target is also a repository-native test. With no argument it uses
// an in-memory registry; pass a facts-tool file registry to query real data.
// It can still be compiled on its own:
//
//   LLVM=$(brew --prefix llvm)
//   $LLVM/bin/clang++ -std=c++23 -I src examples/sqlite_generator_demo.cpp \
//     src/storage/Sqlite.cpp -lsqlite3 -o build/sqlite-generator-demo
//   ./build/sqlite-generator-demo <files.sqlite>

#include "model/FileRecord.h"
#include "storage/SqliteDatabase.h"

#include <algorithm>
#include <print>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace {

facts::FileRecord toRecord(const facts::storage::Row &row) {
  return {static_cast<facts::FileId>(row.integer(0)), row.string(1)};
}

bool isHeader(const facts::FileRecord &record) {
  return record.path.ends_with(".h");
}

} // namespace

int main(int argc, const char **argv) {
  if (argc > 2) {
    std::println(stderr, "usage: sqlite-generator-demo [files.sqlite]");
    return 2;
  }

  const bool selfContained = argc == 1;
  auto database = facts::storage::Database::open(
      selfContained ? ":memory:" : argv[1],
      selfContained ? facts::storage::Database::readWrite
                    : facts::storage::Database::readOnly);
  if (!database) {
    std::println(stderr, "cannot open {}: {}",
                 selfContained ? ":memory:" : argv[1],
                 database.error().message());
    return 1;
  }

  if (selfContained) {
    const auto schema = database->executeCommand(
        "CREATE TABLE file(id INTEGER PRIMARY KEY, path TEXT)");
    auto seed = std::views::iota(0, 4) | std::views::transform([](int index) {
                  return std::pair{
                      index + 1, index % 2 == 0
                                     ? "file" + std::to_string(index) + ".h"
                                     : "file" + std::to_string(index) + ".cpp"};
                });
    const auto inserted =
        seed | database->bulk("INSERT INTO file(id, path) VALUES (?1, ?2)",
                              [](sqlite3_stmt *statement, const auto &file) {
                                return facts::storage::bindParameters(
                                    statement, file.first, file.second);
                              });
    auto transaction = database->write();
    const auto updated = database->executeCommand(
        "UPDATE file SET path = 'src/' || path WHERE id = 1");
    const auto committed = transaction ? transaction->commit()
                                       : std::unexpected(transaction.error());
    const auto removed =
        database->executeCommand("DELETE FROM file WHERE id = 4");
    if (!schema || !inserted || !updated || !committed || !removed) {
      std::println(stderr, "cannot prepare the in-memory demonstration");
      return 1;
    }
  }

  // 1. Rows straight from the database object — no handle, no statement, no
  //    result codes. The cursor lives in the generator's frame.
  std::println("all files");
  for (const auto &row : database->rows("SELECT id, path FROM file "
                                        "ORDER BY id")) {
    std::println("  {:>3}  {}", row.integer(0), row.text(1));
  }

  // 2. query() maps each row while it is still valid, so what comes out owns
  //    itself and composes with views like any other range. Taking two and
  //    stopping destroys the generator, which finalizes the statement
  //    mid-result-set — the case a hand-written loop usually gets wrong.
  std::println("first two headers, by path");
  for (const auto &record :
       database->query("SELECT id, path FROM file ORDER BY path", toRecord) |
           std::views::filter(isHeader) | std::views::take(2)) {
    std::println("  {:>3}  {}", record.id, record.path);
  }

  // 3. Binds are ordinary arguments, and the stream still pipes.
  const auto deepest =
      std::ranges::max(database->query("SELECT id, path FROM file "
                                       "WHERE id >= ?1",
                                       toRecord, facts::firstPhysicalFileId) |
                           std::views::transform(&facts::FileRecord::path),
                       {}, &std::string::size);
  std::println("longest path: {}", deepest);

  // 4. And the eager form, for callers that want a vector and an error value
  //    instead of a stream.
  const auto all = facts::storage::collect(
      database->query("SELECT id, path FROM file", toRecord));
  if (!all) {
    std::println(stderr, "query failed: {}", all.error().message());
    return 1;
  }
  std::println("{} file(s) in the registry", all->size());
  return 0;
}
