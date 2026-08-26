#include "storage/catalog/Component.h"
#include <algorithm>

namespace facts::catalog {

Result<std::filesystem::path> componentRoot(const Component &component) {
  if (component.value.repositoryId &&
      std::filesystem::path(component.value.path).is_relative() &&
      !component.clone) {
    return std::unexpected("component has no active clone: " +
                           component.value.name);
  }
  try {
    return effectiveComponentRoot(component.value, component.clone);
  } catch (const std::filesystem::filesystem_error &error) {
    return std::unexpected(error.what());
  }
}

Result<std::vector<Component>> components(Database &database) {
  return query(
      database,
      "SELECT c.id,c.name,c.path,c.kind,c.version,c.repository_id,"
      "cl.id,cl.repository_id,cl.path,cl.label,r.name,"
      "(SELECT count(*) FROM file f JOIN directory d ON d.id=f.directory_id "
      "WHERE d.component_id=c.id) "
      "FROM component c LEFT JOIN repository r ON r.id=c.repository_id "
      "LEFT JOIN clone cl ON cl.id=r.active_clone_id AND cl.repository_id=r.id "
      "ORDER BY c.id",
      [](const storage::Row &row) {
        Component value;
        value.value = {row.integer(0),
                       row.string(1),
                       row.string(2),
                       row.string(3),
                       row.get<std::optional<std::string>>(4),
                       row.get<std::optional<std::int64_t>>(5)};
        if (!row.isNull(6)) {
          value.clone = ProjectClone{row.integer(6), row.integer(7),
                                     row.string(8), row.string(9)};
        }
        value.repository = row.string(10);
        value.files = row.integer(11);
        return value;
      });
}

Result<Component> component(Database &database, const Selector &selector) {
  return components(database).and_then([&](auto values) -> Result<Component> {
    std::vector<Component> matches;
    for (auto &value : values) {
      const bool identity = selector.id ? value.value.id == *selector.id
                                        : value.value.name == selector.name;
      if (selector.path.empty()) {
        if (identity)
          matches.push_back(std::move(value));
        continue;
      }
      auto root = componentRoot(value);
      if (!root)
        return std::unexpected(root.error());
      std::error_code error;
      const auto requested =
          std::filesystem::weakly_canonical(selector.path, error);
      if (!error && *root == requested)
        matches.push_back(std::move(value));
    }
    return requireOne(std::move(matches), "component");
  });
}

Result<void> setVersion(Database &database, const Component &component,
                        const std::string &version) {
  return validateVersion(version).and_then([&] {
    return execute(database,
                   "UPDATE component SET version=NULLIF(?,'') WHERE id=?",
                   version, component.value.id);
  });
}

Result<void> removeComponent(Database &database, std::int64_t id) {
  return execute(database, "DELETE FROM component WHERE id=?", id);
}
} // namespace facts::catalog
