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
//     src/storage/Sqlite.cpp -lsqlite3 -o build/sqlite-generator-demo
//   ./build/sqlite-generator-demo <files.sqlite>

#include "model/FileRecord.h"
#include "storage/SqliteDatabase.h"

#include <algorithm>
#include <print>
#include <ranges>
#include <string>

namespace {

facts::FileRecord toRecord(const facts::storage::Row &row) {
  return {static_cast<facts::FileId>(row.integer(0)), row.string(1)};
}

bool isHeader(const facts::FileRecord &record) {
  return record.path.ends_with(".h");
}

} // namespace

int main(int argc, const char **argv) {
  if (argc != 2) {
    std::println(stderr, "usage: sqlite-generator-demo <files.sqlite>");
    return 2;
  }

  auto database = facts::storage::Database::open(argv[1]);
  if (!database) {
    std::println(stderr, "cannot open {}: {}", argv[1],
                 database.error().message());
    return 1;
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
  const auto all =
      facts::storage::collect(database->query("SELECT id, path FROM file",
                                              toRecord));
  if (!all) {
    std::println(stderr, "query failed: {}", all.error().message());
    return 1;
  }
  std::println("{} file(s) in the registry", all->size());
  return 0;
}
