#include "storage/FileDatabase.h"
#include "storage/Storage.h"

#include <sqlite3.h>

#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#if defined(__APPLE__)
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#endif

namespace {

bool require(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

std::filesystem::path normalized(const std::filesystem::path &path) {
  std::error_code error;
  auto result = std::filesystem::weakly_canonical(path, error);
  return error ? std::filesystem::absolute(path).lexically_normal() : result;
}

int openDatabaseHandles(const std::string &path) {
  const auto expected = normalized(path);
  int count = 0;
#if defined(__APPLE__)
  for (int descriptor = 0; descriptor < getdtablesize(); ++descriptor) {
    std::array<char, PATH_MAX> resolved{};
    if (fcntl(descriptor, F_GETPATH, resolved.data()) == 0 &&
        normalized(resolved.data()) == expected) {
      ++count;
    }
  }
#elif defined(__linux__)
  std::error_code error;
  for (const auto &entry :
       std::filesystem::directory_iterator("/proc/self/fd", error)) {
    const auto linked = std::filesystem::read_symlink(entry.path(), error);
    if (!error && normalized(linked) == expected) {
      ++count;
    }
    error.clear();
  }
#else
#error "FileDatabasePersistenceTest requires a database-handle counter"
#endif
  return count;
}

bool execute(sqlite3 *database, std::string_view sql) {
  char *message = nullptr;
  const auto status =
      sqlite3_exec(database, sql.data(), nullptr, nullptr, &message);
  if (status == SQLITE_OK) {
    return true;
  }
  std::cerr << (message ? message : sqlite3_errmsg(database)) << '\n';
  sqlite3_free(message);
  return false;
}

std::int64_t scalar(sqlite3 *database, std::string_view sql) {
  sqlite3_stmt *statement = nullptr;
  if (sqlite3_prepare_v2(database, sql.data(), -1, &statement, nullptr) !=
      SQLITE_OK) {
    return -1;
  }
  const auto value = sqlite3_step(statement) == SQLITE_ROW
                         ? sqlite3_column_int64(statement, 0)
                         : -1;
  sqlite3_finalize(statement);
  return value;
}

bool withDatabase(const std::string &path, const auto &inspect) {
  sqlite3 *database = nullptr;
  if (!require(sqlite3_open(path.c_str(), &database) == SQLITE_OK,
               "failed to inspect file database")) {
    return false;
  }
  const auto result = inspect(database);
  sqlite3_close(database);
  return result;
}

bool verifyStorageConnection(const std::string &path) {
  std::filesystem::remove(path);
  facts::Storage storage{path};
  const auto handles = openDatabaseHandles(path);
  return require(handles == 1, "Storage retained " + std::to_string(handles) +
                                   " SQLite connections instead of one");
}

bool verifyFileDatabase(const std::string &path) {
  std::filesystem::remove(path);
  facts::FileDatabase files{path};
  const auto handles = openDatabaseHandles(path);
  if (!require(handles == 1, "FileDatabase retained " +
                                 std::to_string(handles) +
                                 " SQLite connections instead of one")) {
    return false;
  }

  const std::array saved{std::string{"/saved/item.cpp"}};
  const auto firstAdd = files.addBulk(saved);
  if (!require(firstAdd && *firstAdd == 1, "failed to save one file")) {
    return false;
  }
  const auto firstId = files.getId(saved.front());
  if (!require(firstId.has_value(), "failed to load saved file")) {
    return false;
  }
  const auto repeatedAdd = files.addBulk(saved);
  if (!require(repeatedAdd && *repeatedAdd == 0,
               "repeated upsert reported a new file") ||
      !require(files.getId(saved.front()) == firstId,
               "file update changed identity")) {
    return false;
  }
  const auto neverStored = files.getId("/saved/missing.cpp");
  if (!require(!neverStored && neverStored.error() ==
                                   std::make_error_code(
                                       std::errc::no_such_file_or_directory),
               "missing lookup did not return the expected error")) {
    return false;
  }

  const auto deleted = withDatabase(path, [&](sqlite3 *database) {
    return execute(database,
                   "DELETE FROM file WHERE id=" + std::to_string(*firstId));
  });
  const auto missing = files.getId(saved.front());
  if (!require(deleted, "failed to delete saved file") ||
      !require(!missing &&
                   missing.error() == std::make_error_code(
                                          std::errc::no_such_file_or_directory),
               "deleted file did not return the expected error")) {
    return false;
  }

  if (!withDatabase(path, [](sqlite3 *database) {
        return execute(database, R"sql(
CREATE TRIGGER fail_bulk_file BEFORE INSERT ON file
WHEN NEW.name='fail.cpp'
BEGIN
  SELECT RAISE(ABORT, 'forced bulk failure');
END;
)sql");
      })) {
    return false;
  }

  const std::array failing{std::string{"/rollback/new/ok.cpp"},
                           std::string{"/rollback/new/fail.cpp"}};
  if (!require(!files.addBulk(failing).has_value(),
               "failing file batch was accepted")) {
    return false;
  }
  return withDatabase(path, [](sqlite3 *database) {
    return require(scalar(database, "SELECT COUNT(*) FROM file WHERE name IN "
                                    "('ok.cpp','fail.cpp')") == 0,
                   "failing file batch retained file rows") &&
           require(scalar(database, "SELECT COUNT(*) FROM directory WHERE "
                                    "path='rollback/new'") == 0,
                   "failing file batch retained its directory row");
  });
}

} // namespace

int main(int argc, char **argv) {
  if (!require(argc == 3,
               "usage: file-database-persistence-test STORAGE_DB FILE_DB")) {
    return 1;
  }
  return verifyStorageConnection(argv[1]) && verifyFileDatabase(argv[2]) ? 0
                                                                         : 1;
}
