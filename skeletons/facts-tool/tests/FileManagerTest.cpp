#include "storage/FileManager.h"

#include <sqlite3.h>

#include <cassert>
#include <filesystem>
#include <future>
#include <string>
#include <vector>

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

} // namespace

int main(int argc, char **argv) {
  assert(argc == 4);
  const std::filesystem::path databasePath = argv[1];
  const std::filesystem::path source = std::filesystem::canonical(argv[2]);
  const std::filesystem::path header = std::filesystem::canonical(argv[3]);
  std::filesystem::remove(databasePath);

  const std::vector<std::string> paths = {source.string(), header.string()};
  facts::FileManager first(databasePath.string());
  facts::FileManager second(databasePath.string());
  auto firstImport =
      std::async(std::launch::async, [&] { return first.addBulk(paths); });
  auto secondImport =
      std::async(std::launch::async, [&] { return second.addBulk(paths); });
  assert(firstImport.get());
  assert(secondImport.get());

  const auto firstHeader = first.getId(header.string());
  const auto secondHeader = second.getId(header.string());
  assert(firstHeader && secondHeader && *firstHeader == *secondHeader);
  assert(*firstHeader >= facts::firstPhysicalFileId);

  const auto unseen =
      first.getId(std::filesystem::canonical(__FILE__).string());
  assert(!unseen);

  sqlite3 *database = nullptr;
  assert(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK);
  assert(scalar(database, "SELECT COUNT(*) FROM file WHERE id=0") == 0);
  assert(scalar(database, "SELECT COUNT(*) FROM file") == 2);
  assert(scalar(database,
                "SELECT COUNT(*) FROM file WHERE path NOT GLOB '/*'") == 0);
  sqlite3_close(database);
}
