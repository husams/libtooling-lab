#include "storage/Storage.h"

#include "storage/Schema.h"
#include "storage/Sqlite.h"

#include <sqlite3.h>

#include <stdexcept>
#include <utility>

namespace facts {

struct Storage::Connection {
  explicit Connection(const std::string &path) {
    constexpr int flags =
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2(path.c_str(), &database, flags, nullptr) != SQLITE_OK) {
      const std::string message =
          database ? sqlite3_errmsg(database) : "cannot allocate SQLite handle";
      close();
      throw std::runtime_error(message);
    }
    sqlite3_busy_timeout(database, 10000);

    auto initialized =
        storage::execute(database, "PRAGMA foreign_keys=ON;").and_then([this] {
          return storage::execute(database, schemaSql);
        });
    if (!initialized) {
      const std::string message = sqlite3_errmsg(database);
      close();
      throw std::runtime_error(message);
    }
  }

  ~Connection() { close(); }

  void close() {
    if (database) {
      sqlite3_close(database);
      database = nullptr;
    }
  }

  sqlite3 *database = nullptr;
};

Storage::Storage(std::string path)
    : connection_(std::make_unique<Connection>(std::move(path))) {}

Storage::~Storage() = default;

sqlite3 *Storage::handle() const { return connection_->database; }

} // namespace facts
