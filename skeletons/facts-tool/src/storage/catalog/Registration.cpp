#include "storage/catalog/Component.h"
#include "storage/catalog/Repository.h"
#include <algorithm>

namespace facts::catalog {
namespace {

std::filesystem::path registrationRoot(std::filesystem::path path, bool noGit) {
  if (noGit)
    return path;
  for (auto parent = path; !parent.empty(); parent = parent.parent_path()) {
    std::error_code error;
    if (std::filesystem::exists(parent / ".git", error))
      return parent;
    if (parent == parent.parent_path())
      break;
  }
  return path;
}

Result<void> requireNewComponent(Database &database, const std::string &name,
                                 const std::filesystem::path &path) {
  if (name.empty())
    return std::unexpected("component name must not be empty");
  return components(database).and_then([&](const auto &values) -> Result<void> {
    for (const auto &value : values) {
      auto root = componentRoot(value);
      if (!root)
        return std::unexpected(root.error());
      if (value.value.name == name || *root == path) {
        return std::unexpected("component name or path already registered");
      }
    }
    return {};
  });
}

Result<Repository> ensureRepository(Database &database, const std::string &name,
                                    const std::string &path) {
  return execute(database,
                 "INSERT INTO repository(name) VALUES(?) ON CONFLICT(name) DO "
                 "NOTHING",
                 name)
      .and_then([&] { return repository(database, name); })
      .and_then([&](Repository repo) -> Result<Repository> {
        if (repo.activeCloneId)
          return repo;
        return addClone(database, repo, path, "").and_then([&] {
          return repository(database, name);
        });
      });
}

Result<void> insertComponent(Database &database,
                             const ComponentRegistration &options,
                             const std::string &name,
                             const std::filesystem::path &root) {
  const auto insert = [&](const std::string &path,
                          std::optional<std::int64_t> repo) {
    return execute(
        database,
        "INSERT INTO component(name,path,kind,version,repository_id) "
        "VALUES(?,?,?,NULLIF(?,''),?)",
        name, path, options.kind, options.version, repo);
  };
  const auto repositoryName =
      options.repository.empty() && options.kind == "repo" ? name
                                                           : options.repository;
  if (repositoryName.empty())
    return insert(root.string(), std::nullopt);
  return ensureRepository(database, repositoryName, root.string())
      .and_then([&](const Repository &repo) -> Result<void> {
        const auto relative = root.lexically_relative(repo.activePath);
        if (relative.empty() || relative.is_absolute() ||
            *relative.begin() == "..") {
          return std::unexpected(
              "component path is outside the repository's active clone");
        }
        return insert(relative.generic_string(), repo.id);
      });
}
} // namespace

Result<void> addComponent(Database &database,
                          const ComponentRegistration &options) {
  return validateVersion(options.version)
      .and_then([&] { return existingDirectory(options.path); })
      .transform([&](auto path) {
        return registrationRoot(path,
                                options.noGit || options.kind == "external");
      })
      .and_then([&](const auto &root) {
        const auto name =
            options.name.empty() ? root.filename().string() : options.name;
        return requireNewComponent(database, name, root).and_then([&] {
          return insertComponent(database, options, name, root);
        });
      });
}
} // namespace facts::catalog
