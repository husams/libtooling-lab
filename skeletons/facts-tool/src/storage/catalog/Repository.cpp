#include "storage/catalog/Repository.h"
#include <algorithm>

namespace facts::catalog {

Result<std::vector<Repository>> repositories(Database &database) {
  return query(
      database,
      "SELECT r.id,r.name,r.kind,r.remote_url,r.active_clone_id,c.path,"
      "(SELECT count(*) FROM component WHERE repository_id=r.id),"
      "(SELECT count(*) FROM clone WHERE repository_id=r.id) "
      "FROM repository r LEFT JOIN clone c ON c.id=r.active_clone_id ORDER BY "
      "r.id",
      [](const storage::Row &row) {
        return Repository{row.integer(0),
                          row.string(1),
                          row.string(2),
                          row.string(3),
                          row.get<std::optional<std::int64_t>>(4),
                          row.string(5),
                          row.integer(6),
                          row.integer(7)};
      });
}

Result<Repository> repository(Database &database, const std::string &name) {
  return repositories(database).and_then([&](auto values) {
    std::erase_if(values,
                  [&](const auto &value) { return value.name != name; });
    return requireOne(std::move(values), "repository '" + name + "'");
  });
}

Result<std::vector<ProjectClone>> clones(Database &database,
                                         std::int64_t repositoryId) {
  return query(
      database,
      "SELECT id,repository_id,path,label FROM clone WHERE repository_id=? "
      "ORDER BY id",
      [](const storage::Row &row) {
        return ProjectClone{row.integer(0), row.integer(1), row.string(2),
                            row.string(3)};
      },
      repositoryId);
}
} // namespace facts::catalog
