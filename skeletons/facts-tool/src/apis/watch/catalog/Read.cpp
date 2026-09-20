#include "apis/watch/catalog/Catalog.h"
#include "apis/watch/catalog/Exclusions.h"
#include "config/Configuration.h"
#include "storage/catalog/Database.h"

namespace facts::apis::watch {
namespace {
config::Request request(const Settings &settings) {
  config::Request result;
  for (std::size_t index = 0; index + 1 < settings.defaults.size(); index += 2) {
    const auto &option = settings.defaults[index];
    if (option == "--config") result.selector = settings.defaults[index + 1];
    if (option == "--conf") result.direct = settings.defaults[index + 1];
  }
  return result;
}
std::expected<std::vector<Clone>, std::string>
readClones(const std::filesystem::path &path, const Settings &settings) {
  std::error_code error;
  const bool exists = std::filesystem::exists(path, error);
  if (error) return std::unexpected("cannot inspect project database: " + error.message());
  if (!exists) return std::vector<Clone>{};
  return catalog::open(path.string(), false)
      .and_then([&](catalog::Database database) {
        return catalog::query(database,
            "SELECT r.id,c.id,r.name,coalesce(c.label,''),c.path,"
            "coalesce(r.active_clone_id=c.id,0) FROM repository r "
            "JOIN clone c ON c.repository_id=r.id ORDER BY r.id,c.id",
            [&](const storage::Row &row) {
              Clone clone{row.integer(0), row.integer(1), row.string(2),
                          row.string(3), row.string(4), row.integer(5) != 0, {}};
              clone.excluded = exclusion(clone, settings);
              return clone;
            });
      });
}
}
std::expected<Catalog, std::string> readCatalog(const Settings &settings) {
  return config::resolve(request(settings))
      .and_then([&](const config::Resolved &resolved) {
        return readClones(resolved.database, settings)
            .transform([&](std::vector<Clone> clones) {
              return Catalog{resolved.database, std::move(clones)};
            });
      });
}
}
