#include "apis/index/Internal.h"

namespace facts::apis::index {
Result<void> stage(Database &database, storage::Statement &statement,
                   const std::string &usr, const std::string &name,
                   std::int64_t file, std::int64_t kind, bool definition,
                   const std::string &path, const std::string &source) {
  if (file <= 0 || usr.empty()) return {};
  return kindName(kind).and_then([&](const std::string &semantic) -> Result<void> {
    sqlite3_reset(statement.get());
    sqlite3_clear_bindings(statement.get());
    if (!storage::bindParameters(statement.get(), usr, name, file, semantic,
                                 definition ? 1 : 0, path, source) ||
        sqlite3_step(statement.get()) != SQLITE_DONE)
      return std::unexpected(catalog::databaseError(database));
    return {};
  });
}
}
