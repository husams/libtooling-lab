#include "commands/CallGraphInvalidation.h"

#include "storage/catalog/Database.h"

namespace facts::commands {

std::expected<bool, std::string>
callGraphProjectHasFiles(const std::string &configuration) {
  return storage::Database::open(configuration, storage::Database::readOnly)
      .transform_error([](std::error_code error) { return error.message(); })
      .and_then([](auto database) -> std::expected<bool, std::string> {
        auto tables = catalog::query(
            database,
            "SELECT 1 FROM sqlite_master WHERE type='table' AND name='file'",
            [](const storage::Row &) { return true; });
        if (!tables)
          return std::unexpected(tables.error());
        // Generated ownership can exist before catalog initialization.
        if (tables->empty())
          return false;
        return catalog::query(database, "SELECT 1 FROM file LIMIT 1",
                              [](const storage::Row &) { return true; })
            .transform([](const auto &rows) { return !rows.empty(); });
      });
}
} // namespace facts::commands
