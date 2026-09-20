#include "apis/index/Internal.h"
#include <set>

namespace facts::apis::index {
Result<std::vector<std::filesystem::path>> sources(
    Database &database, const std::filesystem::path &project) {
  return catalog::query(database,
      "SELECT DISTINCT facts_db FROM file WHERE facts_db IS NOT NULL "
      "AND facts_db<>'' ORDER BY facts_db",
      [](const storage::Row &row) { return row.string(0); })
      .and_then([&](const auto &paths)
          -> Result<std::vector<std::filesystem::path>> {
        std::set<std::filesystem::path> unique;
        for (const auto &value : paths) {
          auto path = std::filesystem::path(value);
          if (path.is_relative()) path = project.parent_path() / path;
          std::error_code error;
          path = std::filesystem::weakly_canonical(path, error);
          if (error) return std::unexpected("cannot resolve facts database: " + error.message());
          unique.insert(std::move(path));
        }
        return std::vector<std::filesystem::path>(unique.begin(), unique.end());
      });
}
}
