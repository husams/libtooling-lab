#include "storage/catalog/Component.h"
#include "storage/catalog/Repository.h"
#include <algorithm>

namespace facts::catalog {
namespace {

Result<void> validateClone(Database &database, const Repository &repo,
                           const std::string &path, const std::string &label) {
  return query(
             database,
             "SELECT id FROM clone WHERE (path=? AND repository_id!=?) OR "
             "(repository_id=? AND label=? AND ?!='' AND path!=?)",
             [](const storage::Row &row) { return row.integer(0); }, path,
             repo.id, repo.id, label, label, path)
      .and_then([](const auto &conflicts) -> Result<void> {
        if (!conflicts.empty())
          return std::unexpected("clone path or label already registered");
        return {};
      });
}

Result<void> validateCloneFiles(Database &database, const Repository &repo,
                                const ProjectClone &clone) {
  return components(database).and_then([&](auto values) -> Result<void> {
    for (auto &component : values) {
      if (component.value.repositoryId != repo.id)
        continue;
      component.clone = clone;
      auto root = componentRoot(component);
      if (!root)
        return std::unexpected(root.error());
      auto files = query(
          database,
          "SELECT d.path,f.name FROM file f JOIN directory d ON "
          "d.id=f.directory_id "
          "WHERE d.component_id=?",
          [&](const storage::Row &row) {
            return *root / row.string(0) / row.string(1);
          },
          component.value.id);
      if (!files)
        return std::unexpected(files.error());
      for (const auto &file : *files) {
        std::error_code error;
        if (!std::filesystem::is_regular_file(file, error)) {
          return std::unexpected("clone is missing registered file: " +
                                 file.string());
        }
      }
    }
    return {};
  });
}
} // namespace

Result<void> addClone(Database &database, const Repository &repo,
                      const std::string &path, const std::string &label) {
  return existingDirectory(path).and_then([&](const auto &canonical) {
    const auto normalized = canonical.string();
    return validateClone(database, repo, normalized, label)
        .and_then([&] {
          return execute(database,
                         "INSERT INTO clone(repository_id,path,label) "
                         "VALUES(?,?,NULLIF(?,'')) "
                         "ON CONFLICT(path) DO UPDATE SET "
                         "label=coalesce(excluded.label,clone.label)",
                         repo.id, normalized, label);
        })
        .and_then([&] {
          return execute(database,
                         "UPDATE repository SET active_clone_id=(SELECT id "
                         "FROM clone WHERE path=?) "
                         "WHERE id=? AND active_clone_id IS NULL",
                         normalized, repo.id);
        });
  });
}

Result<void> switchClone(Database &database, const Repository &repo,
                         const std::string &target) {
  return clones(database, repo.id)
      .and_then([&](auto values) {
        std::error_code error;
        const auto path = std::filesystem::canonical(target, error).string();
        std::erase_if(values, [&](const auto &clone) {
          return clone.label != target && clone.path != target &&
                 (error || clone.path != path);
        });
        return requireOne(std::move(values), "clone '" + target + "'");
      })
      .and_then([&](const ProjectClone &clone) {
        return existingDirectory(clone.path)
            .and_then([&](const auto &) {
              return validateCloneFiles(database, repo, clone);
            })
            .and_then([&] {
              return execute(
                  database,
                  "UPDATE repository SET active_clone_id=? WHERE id=?",
                  clone.id, repo.id);
            });
      });
}
} // namespace facts::catalog
