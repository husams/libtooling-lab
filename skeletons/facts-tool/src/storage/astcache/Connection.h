#pragma once

#include "storage/astcache/Database.h"
#include "storage/ProjectSchema.h"
#include "storage/catalog/Database.h"

#include <type_traits>

namespace facts::storage::astcache::detail {

template <class T> using Result = std::expected<T, std::string>;

Result<storage::Database> open(const std::filesystem::path &path, bool writable);
Result<void> storeSnapshot(storage::Database &database,
                           const facts::astcache::Snapshot &snapshot);

// The schema check and every row read/write share a single SQLite snapshot.
// A failed stage leaves the transaction scope to roll back automatically.
template <class Work>
auto transact(storage::Database &database, bool writable, Work work)
    -> decltype(work()) {
  using Value = typename decltype(work())::value_type;
  return database.transaction(writable ? TransactionMode::immediate
                                       : TransactionMode::deferred)
      .transform_error([&](auto) { return catalog::databaseError(database); })
      .and_then([&](Transaction transaction) -> decltype(work()) {
        return requireCurrentProjectSchema(database.nativeHandle())
            .and_then(work)
            .and_then([&](auto &&...value) {
              return transaction.commit()
                  .transform_error([&](auto) { return catalog::databaseError(database); })
                  .transform([&]() -> Value {
                    if constexpr (!std::is_void_v<Value>)
                      return (std::move(value), ...);
                  });
            });
      });
}

} // namespace facts::storage::astcache::detail
