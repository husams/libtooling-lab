#include "storage/astcache/Connection.h"

namespace facts::storage::astcache {

std::expected<void, std::string>
clearSnapshots(const std::filesystem::path &project) {
  std::error_code error;
  const bool exists = std::filesystem::exists(project, error);
  if (error)
    return std::unexpected("cannot inspect project configuration " +
                           project.string() + ": " + error.message());
  if (!exists) return {};
  return detail::open(project, true)
      .and_then([&](storage::Database database) {
        return detail::transact(database, true, [&] {
          // Foreign keys cascade to dependency and artifact metadata together.
          return catalog::execute(database, "DELETE FROM ast_cache_snapshot");
        });
      });
}

} // namespace facts::storage::astcache
