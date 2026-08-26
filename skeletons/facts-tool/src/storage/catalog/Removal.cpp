#include "storage/catalog/Component.h"
#include "storage/catalog/Repository.h"

namespace facts::catalog {

Result<void> detachComponents(Database &database, std::int64_t repositoryId) {
  return components(database).and_then([&](auto values) -> Result<void> {
    for (auto &component : values) {
      if (component.value.repositoryId != repositoryId)
        continue;
      // Persist the base without the version, which remains its own path
      // segment.
      component.value.version.reset();
      auto detached = componentRoot(component).and_then([&](const auto &root) {
        return execute(
            database,
            "UPDATE component SET path=?,repository_id=NULL WHERE id=?",
            root.string(), component.value.id);
      });
      if (!detached)
        return std::unexpected(detached.error());
    }
    return {};
  });
}

Result<void> removeRepository(Database &database, const Repository &repository,
                              bool deleteComponents) {
  const auto prepare =
      deleteComponents
          ? execute(database, "DELETE FROM component WHERE repository_id=?",
                    repository.id)
          : detachComponents(database, repository.id);
  return prepare.and_then([&] {
    return execute(database, "DELETE FROM repository WHERE id=?",
                   repository.id);
  });
}
} // namespace facts::catalog
