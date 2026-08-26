#include "storage/catalog/Directory.h"
#include "storage/catalog/Component.h"
#include <algorithm>

namespace facts::catalog {

Result<std::vector<Directory>> directories(Database &database,
                                           const std::string &componentName) {
  const auto read = [&](std::optional<std::int64_t> id) {
    return query(
        database,
        "SELECT d.id,d.component_id,c.name,d.path,"
        "(SELECT count(*) FROM file WHERE directory_id=d.id) "
        "FROM directory d JOIN component c ON c.id=d.component_id "
        "WHERE (? IS NULL OR c.id=?) ORDER BY d.id",
        [](const storage::Row &row) {
          return Directory{row.integer(0), row.integer(1), row.string(2),
                           row.string(3), row.integer(4)};
        },
        id, id);
  };
  if (componentName.empty())
    return read(std::nullopt);
  return component(database, {.name = componentName})
      .and_then([&](const Component &value) { return read(value.value.id); });
}

Result<Directory> directory(Database &database, const Selector &selector,
                            const std::string &componentName) {
  return directories(database, componentName).and_then([&](auto values) {
    const auto path = std::filesystem::path(selector.path)
                          .lexically_normal()
                          .generic_string();
    std::erase_if(values, [&](const auto &value) {
      return selector.id ? value.id != *selector.id : value.path != path;
    });
    return requireOne(std::move(values), "directory");
  });
}

Result<void> removeDirectory(Database &database, std::int64_t id) {
  return execute(database, "DELETE FROM directory WHERE id=?", id);
}
} // namespace facts::catalog
