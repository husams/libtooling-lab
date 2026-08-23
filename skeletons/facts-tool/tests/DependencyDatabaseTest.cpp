#include "storage/DependencyDatabase.h"
#include "storage/FileManager.h"

#include <sqlite3.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace {

void require(bool condition) {
  if (!condition) {
    std::abort();
  }
}

long long scalar(sqlite3 *database, const char *sql, facts::FileId source = 0,
                 facts::FileId destination = 0) {
  sqlite3_stmt *statement = nullptr;
  require(sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) ==
          SQLITE_OK);
  if (source != 0) {
    require(sqlite3_bind_int64(statement, 1, source) == SQLITE_OK);
  }
  if (destination != 0) {
    require(sqlite3_bind_int64(statement, 2, destination) == SQLITE_OK);
  }
  require(sqlite3_step(statement) == SQLITE_ROW);
  const auto result = sqlite3_column_int64(statement, 0);
  sqlite3_finalize(statement);
  return result;
}

} // namespace

int main(int argc, char **argv) {
  require(argc == 5);
  const auto databasePath = std::filesystem::absolute(argv[1]).string();
  const auto root = std::filesystem::canonical(argv[2]).string();
  const auto middle = std::filesystem::canonical(argv[3]).string();
  const auto leaf = std::filesystem::canonical(argv[4]).string();

  facts::FileManager files(databasePath);
  const auto rootId = files.getId(root);
  const auto middleId = files.getId(middle);
  const auto leafId = files.getId(leaf);
  require(rootId && middleId && leafId);

  sqlite3 *database = nullptr;
  require(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK);
  require(scalar(database, "SELECT COUNT(*) FROM include_dependency") == 2);
  require(scalar(database,
                 "SELECT COUNT(*) FROM include_dependency "
                 "WHERE src_file_id=?1 AND dst_file_id=?2",
                 *rootId, *middleId) == 1);
  require(scalar(database,
                 "SELECT COUNT(*) FROM include_dependency "
                 "WHERE src_file_id=?1 AND dst_file_id=?2",
                 *middleId, *leafId) == 1);
  require(scalar(database,
                 "SELECT COUNT(*) FROM include_dependency "
                 "WHERE src_file_id=?1 AND dst_file_id=?2",
                 *rootId, *leafId) == 0);
  sqlite3_close(database);

  const std::vector<facts::FileId> visitedSources{*middleId};
  const std::vector<facts::DependencyEdge> replacementEdges;
  require(
      facts::replaceDependencies(databasePath, visitedSources, replacementEdges)
          .has_value());

  require(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK);
  require(scalar(database, "SELECT COUNT(*) FROM include_dependency") == 1);
  require(scalar(database,
                 "SELECT COUNT(*) FROM include_dependency "
                 "WHERE src_file_id=?1 AND dst_file_id=?2",
                 *rootId, *middleId) == 1);
  require(scalar(database,
                 "SELECT COUNT(*) FROM include_dependency "
                 "WHERE src_file_id=?1 AND dst_file_id=?2",
                 *middleId, *leafId) == 0);
  sqlite3_close(database);
}
