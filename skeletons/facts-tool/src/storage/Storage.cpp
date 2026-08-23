#include "storage/Storage.h"

#include "storage/Schema.h"
#include "storage/SchemaMigration.h"
#include <sqlite3.h>

#include <stdexcept>
#include <utility>

namespace facts {

namespace {

storage::Database openDatabase(const std::string &path) {
  constexpr int flags = storage::Database::readWrite | SQLITE_OPEN_FULLMUTEX;
  auto opened = storage::Database::open(path, flags);
  if (!opened) {
    throw std::system_error(opened.error(), "cannot open SQLite database");
  }

  auto database = std::move(*opened);
  auto initialized =
      database.executeScript("PRAGMA foreign_keys=OFF;")
          .and_then([&] { return database.write(); })
          .and_then([&](storage::Transaction transaction) {
            return storage::migrateSchema(database.nativeHandle())
                .and_then([&] { return database.executeScript(schemaSql); })
                .and_then([&] { return transaction.commit(); });
          })
          .and_then([&] {
            return database.executeScript("PRAGMA foreign_keys=ON;");
          });
  if (!initialized) {
    throw std::system_error(initialized.error(),
                            "cannot initialize SQLite database");
  }
  return database;
}

} // namespace

Storage::Storage(std::string path) : database_(openDatabase(path)) {}

Storage::~Storage() = default;

sqlite3 *Storage::handle() const { return database_.nativeHandle(); }

} // namespace facts
