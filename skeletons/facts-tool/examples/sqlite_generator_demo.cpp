// sqlite_generator_demo — the coroutine query API against a file registry.
//
// The registry is the SQLite database facts-tool writes with --files-out: one
// `file(id, path)` row per translation unit and header it saw.
//
// This example is deliberately outside the CMake build, so it can be compiled
// on its own:
//
//   LLVM=$(brew --prefix llvm)
//   $LLVM/bin/clang++ -std=c++23 -I src examples/sqlite_generator_demo.cpp \
//     -lsqlite3 -o build/sqlite-generator-demo
//   ./build/sqlite-generator-demo <files.sqlite>

#include "model/FileRecord.h"
#include "storage/SqliteConnection.h"
#include "storage/SqliteQuery.h"

#include <print>
#include <string>

namespace {

facts::FileRecord toRecord(const facts::sql::Row &row) {
  return {static_cast<facts::FileId>(row.integer(0)), row.string(1)};
}

} // namespace

int main(int argc, const char **argv) {
  if (argc != 2) {
    std::println(stderr, "usage: sqlite-generator-demo <files.sqlite>");
    return 2;
  }

  auto connection = facts::sql::Connection::open(argv[1]);
  if (!connection) {
    std::println(stderr, "cannot open {}: {}", argv[1],
                 connection.error().message());
    return 1;
  }
  auto *database = connection->handle();

  // 1. The raw stream: rows arrive one at a time, nothing is materialised.
  std::println("all files");
  for (const auto &row : facts::sql::rows(
           database, "SELECT id, path FROM file ORDER BY id")) {
    if (!row) {
      std::println(stderr, "query failed: {}", row.error().message());
      return 1;
    }
    std::println("  {:>3}  {}", row->integer(0), row->text(1));
  }

  // 2. Mapped to a domain type, with a bound parameter. Breaking out early
  //    destroys the generator, which finalises the statement mid-result-set —
  //    the case the hand-written loop usually gets wrong.
  std::println("first header");
  for (const auto &record :
       facts::sql::select(database,
                          "SELECT id, path FROM file "
                          "WHERE path LIKE ?1 ORDER BY path",
                          toRecord, std::string("%.h"))) {
    if (!record) {
      std::println(stderr, "query failed: {}", record.error().message());
      return 1;
    }
    std::println("  {:>3}  {}", record->id, record->path);
    break;
  }

  // 3. And the eager form, for callers that really do want the whole vector.
  const auto all = facts::sql::collect(facts::sql::select(
      database, "SELECT id, path FROM file", toRecord));
  if (!all) {
    std::println(stderr, "query failed: {}", all.error().message());
    return 1;
  }
  std::println("{} file(s) in the registry", all->size());
  return 0;
}
