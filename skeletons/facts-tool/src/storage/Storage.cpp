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
    throw std::runtime_error("cannot open SQLite database: " +
                             opened.error().message());
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
    throw std::runtime_error("cannot initialize SQLite database '" + path +
                             "': " + initialized.error().message());
  }
  return database;
}

} // namespace

Storage::Storage(std::string path) : database_(openDatabase(path)) {}

Storage::~Storage() = default;

std::expected<void, std::error_code> Storage::begin() {
  if (transaction_) {
    return std::unexpected(
        std::make_error_code(std::errc::operation_in_progress));
  }
  return database_.write().transform([this](storage::Transaction transaction) {
    transaction_.emplace(std::move(transaction));
    database_.setNestedBulkAtomic(false);
  });
}

std::expected<void, std::error_code> Storage::commit() {
  if (!transaction_) {
    return std::unexpected(
        std::make_error_code(std::errc::operation_not_permitted));
  }
  return transaction_->commit().transform([this] {
    transaction_.reset();
    database_.setNestedBulkAtomic(true);
  });
}

std::expected<void, std::error_code> Storage::rollback() {
  if (!transaction_) {
    return std::unexpected(
        std::make_error_code(std::errc::operation_not_permitted));
  }
  return transaction_->rollback().transform([this] {
    transaction_.reset();
    database_.setNestedBulkAtomic(true);
  });
}

std::expected<Storage::OptionalTransaction, std::error_code>
Storage::readTransaction() {
  if (transaction_) {
    return OptionalTransaction{};
  }
  return database_.read().transform([](storage::Transaction transaction) {
    return OptionalTransaction{std::move(transaction)};
  });
}

std::expected<Storage::OptionalTransaction, std::error_code>
Storage::writeTransaction() {
  if (transaction_) {
    return OptionalTransaction{};
  }
  return database_.write().transform([](storage::Transaction transaction) {
    return OptionalTransaction{std::move(transaction)};
  });
}

std::expected<void, std::error_code>
Storage::commit(OptionalTransaction &transaction) {
  return transaction ? transaction->commit()
                     : std::expected<void, std::error_code>{};
}

sqlite3 *Storage::handle() const { return database_.nativeHandle(); }

} // namespace facts
