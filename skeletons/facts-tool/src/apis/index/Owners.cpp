#include "apis/index/Internal.h"
#include "storage/catalog/File.h"

namespace facts::apis::index {
Result<void> prepareOwners(Database &database, const std::filesystem::path &base) {
  return catalog::files(database).and_then([&](const auto &files) -> Result<void> {
    for (const auto &file : files) {
      if (file.factsDb.empty()) continue;
      std::filesystem::path path = file.factsDb;
      const auto root = file.clone ? std::filesystem::path(file.clone->path) : base;
      if (path.is_relative()) path = root / path;
      std::error_code error;
      path = std::filesystem::weakly_canonical(path, error);
      if (error) return std::unexpected("cannot resolve facts owner " + path.string() + ": " + error.message());
      auto result = catalog::execute(database,
          "INSERT INTO temp.api_symbol_owner(file_id,facts_db) VALUES(?,?)", file.id, path.string());
      if (!result) return result;
    }
    return {};
  });
}
}
