#include "storage/DependencyDatabase.h"
#include "storage/FileManager.h"

#include <sqlite3.h>

#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

namespace {

long long scalar(sqlite3 *database, const char *sql, facts::FileId source = 0,
                 facts::FileId destination = 0) {
  sqlite3_stmt *statement = nullptr;
  assert(sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) ==
         SQLITE_OK);
  if (source != 0) {
    assert(sqlite3_bind_int64(statement, 1, source) == SQLITE_OK);
  }
  if (destination != 0) {
    assert(sqlite3_bind_int64(statement, 2, destination) == SQLITE_OK);
  }
  assert(sqlite3_step(statement) == SQLITE_ROW);
  const auto result = sqlite3_column_int64(statement, 0);
  sqlite3_finalize(statement);
  return result;
}

} // namespace

int main(int argc, char **argv) {
  assert(argc == 5);
  const auto databasePath = std::filesystem::absolute(argv[1]).string();
  const auto root = std::filesystem::canonical(argv[2]).string();
  const auto middle = std::filesystem::canonical(argv[3]).string();
  const auto leaf = std::filesystem::canonical(argv[4]).string();

  facts::FileManager files(databasePath);
  const auto rootId = files.getId(root);
  const auto middleId = files.getId(middle);
  const auto leafId = files.getId(leaf);
  assert(rootId && middleId && leafId);

  sqlite3 *database = nullptr;
  assert(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK);
  assert(scalar(database, "SELECT COUNT(*) FROM include_dependency") == 2);
  assert(scalar(database,
                "SELECT COUNT(*) FROM include_dependency "
                "WHERE src_file_id=?1 AND dst_file_id=?2",
                *rootId, *middleId) == 1);
  assert(scalar(database,
                "SELECT COUNT(*) FROM include_dependency "
                "WHERE src_file_id=?1 AND dst_file_id=?2",
                *middleId, *leafId) == 1);
  assert(scalar(database,
                "SELECT COUNT(*) FROM include_dependency "
                "WHERE src_file_id=?1 AND dst_file_id=?2",
                *rootId, *leafId) == 0);
  sqlite3_close(database);

  const std::vector<facts::FileId> visitedSources{*middleId};
  const std::vector<facts::DependencyEdge> replacementEdges;
  assert(facts::replaceDependencies(databasePath, visitedSources,
                                    replacementEdges));

  assert(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK);
  assert(scalar(database, "SELECT COUNT(*) FROM include_dependency") == 1);
  assert(scalar(database,
                "SELECT COUNT(*) FROM include_dependency "
                "WHERE src_file_id=?1 AND dst_file_id=?2",
                *rootId, *middleId) == 1);
  assert(scalar(database,
                "SELECT COUNT(*) FROM include_dependency "
                "WHERE src_file_id=?1 AND dst_file_id=?2",
                *middleId, *leafId) == 0);
  sqlite3_close(database);
}
