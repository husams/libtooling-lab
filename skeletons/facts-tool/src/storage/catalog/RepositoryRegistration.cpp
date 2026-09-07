#include "storage/catalog/Repository.h"
#include <algorithm>

namespace facts::catalog {
namespace {

Result<void> requireNewRepository(Database &database, const std::string &name) {
  if (name.empty())
    return std::unexpected("repository name must not be empty");
  return repositories(database).and_then(
      [&](const auto &values) -> Result<void> {
        const bool taken = std::ranges::any_of(
            values, [&](const auto &value) { return value.name == name; });
        if (taken)
          return std::unexpected("repository '" + name +
                                 "' already registered");
        return {};
      });
}
} // namespace

// Registers a repository together with its first checkout, which becomes the
// active clone. The caller's transaction rolls the repository row back when
// the clone is rejected, so a failed registration leaves no partial state.
Result<Repository> addRepository(Database &database,
                                 const RepositoryRegistration &options) {
  return requireNewRepository(database, options.name)
      .and_then([&] {
        return execute(database,
                       "INSERT INTO repository(name,remote_url) "
                       "VALUES(?,NULLIF(?,''))",
                       options.name, options.remote);
      })
      .and_then([&] { return repository(database, options.name); })
      .and_then([&](const Repository &repo) {
        return addClone(database, repo, options.path, options.label)
            .and_then([&] { return repository(database, options.name); });
      });
}
} // namespace facts::catalog
