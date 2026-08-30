#include "storage/catalog/File.h"
#include "storage/catalog/Component.h"
#include "storage/catalog/Directory.h"
#include <algorithm>

namespace facts::catalog {
namespace {

bool contains(const std::filesystem::path &root,
              const std::filesystem::path &path) {
  auto candidate = path.begin();
  return std::ranges::all_of(root, [&](const auto &segment) {
    if (candidate == path.end() || *candidate != segment)
      return false;
    ++candidate;
    return true;
  });
}

struct OwnedDirectory {
  Directory value;
  std::filesystem::path root;
};

Result<std::filesystem::path> regularFile(const std::string &path,
                                          std::string_view description) {
  std::error_code error;
  auto canonical = std::filesystem::canonical(path, error);
  if (error || !std::filesystem::is_regular_file(canonical, error)) {
    return std::unexpected(std::string(description) +
                           " does not exist or is invalid: " + path);
  }
  return canonical;
}

Result<OwnedDirectory> owningDirectory(Database &database,
                                       const std::filesystem::path &path) {
  return components(database).and_then([&](const auto &componentValues) {
    return directories(database, "")
        .and_then([&](auto directoryValues) -> Result<OwnedDirectory> {
          std::optional<std::pair<std::size_t, OwnedDirectory>> selected;
          for (const auto &component : componentValues) {
            auto root = componentRoot(component);
            if (!root)
              return std::unexpected(root.error());
            for (const auto &directory : directoryValues) {
              if (directory.componentId != component.value.id)
                continue;
              const auto candidate =
                  (*root / directory.path).lexically_normal();
              if (!contains(candidate, path))
                continue;
              const auto depth = static_cast<std::size_t>(
                  std::ranges::distance(candidate.begin(), candidate.end()));
              if (!selected || depth > selected->first)
                selected =
                    std::pair{depth, OwnedDirectory{directory, candidate}};
            }
          }
          return selected ? Result<OwnedDirectory>{std::move(selected->second)}
                          : Result<OwnedDirectory>{std::unexpected(
                                "file is outside every registered indexed "
                                "directory")};
        });
  });
}

} // namespace

Result<std::filesystem::path> filePath(const File &file) {
  try {
    return effectiveComponentRoot(file.component, file.clone) / file.directory /
           file.name;
  } catch (const std::filesystem::filesystem_error &error) {
    return std::unexpected(error.what());
  }
}

std::string relativeFilePath(const File &file) {
  return (std::filesystem::path(file.directory) / file.name)
      .lexically_normal()
      .generic_string();
}

Result<std::vector<File>> files(Database &database) {
  return query(
      database,
      "SELECT f.id,f.directory_id,c.id,c.name,c.path,c.kind,c.version,"
      "c.repository_id,cl.id,cl.repository_id,cl.path,cl.label,d.path,f.name,"
      "coalesce(f.compile_options,'[]'),coalesce(f.driver,''),"
      "coalesce(f.working_directory,''),f.args_overridden,f.indexed "
      "FROM file f JOIN directory d ON d.id=f.directory_id "
      "JOIN component c ON c.id=d.component_id "
      "LEFT JOIN repository r ON r.id=c.repository_id "
      "LEFT JOIN clone cl ON cl.id=r.active_clone_id ORDER BY f.id",
      [](const storage::Row &row) {
        File value;
        value.id = row.integer(0);
        value.directoryId = row.integer(1);
        value.component = {row.integer(2),
                           row.string(3),
                           row.string(4),
                           row.string(5),
                           row.get<std::optional<std::string>>(6),
                           row.get<std::optional<std::int64_t>>(7)};
        if (!row.isNull(8)) {
          value.clone = ProjectClone{row.integer(8), row.integer(9),
                                     row.string(10), row.string(11)};
        }
        value.componentName = row.string(3);
        value.directory = row.string(12);
        value.name = row.string(13);
        value.compileOptions = row.string(14);
        value.driver = row.string(15);
        value.workingDirectory = row.string(16);
        value.argsOverridden = row.integer(17) != 0;
        value.indexed = row.integer(18) != 0;
        return value;
      });
}

Result<File> file(Database &database, const std::string &path) {
  std::error_code error;
  const auto requested = std::filesystem::weakly_canonical(path, error);
  if (error)
    return std::unexpected("invalid file path: " + path);
  return files(database).and_then([&](auto values) -> Result<File> {
    std::vector<File> matches;
    for (auto &value : values) {
      auto resolved = filePath(value);
      if (!resolved)
        return std::unexpected(resolved.error());
      if (std::filesystem::weakly_canonical(*resolved) == requested)
        matches.push_back(std::move(value));
    }
    return requireOne(std::move(matches), "file '" + path + "'");
  });
}

Result<void> addFile(Database &database, const std::string &path,
                     const std::string &driver,
                     const std::string &workingDirectory,
                     const std::string &compileOptions) {
  return regularFile(path, "file").and_then([&](const auto &canonical) {
    return regularFile(driver, "compiler driver")
        .and_then([&](const auto &canonicalDriver) {
          return owningDirectory(database, canonical)
              .and_then([&](const OwnedDirectory &directory) {
                auto working =
                    workingDirectory.empty()
                        ? Result<std::filesystem::path>{directory.root}
                        : existingDirectory(workingDirectory);
                return working.and_then([&](const auto &resolvedWorking) {
                  return query(
                             database,
                             "SELECT id FROM file WHERE directory_id=? "
                             "AND name=?",
                             [](const storage::Row &row) {
                               return row.integer(0);
                             },
                             directory.value.id,
                             canonical.filename().generic_string())
                      .and_then([&](const auto &duplicates) -> Result<void> {
                        if (!duplicates.empty())
                          return std::unexpected("file already registered: " +
                                                 canonical.string());
                        return execute(
                            database,
                            "INSERT INTO file(directory_id,name,driver,"
                            "working_directory,compile_options,"
                            "args_overridden) VALUES(?,?,?,?,?,1)",
                            directory.value.id,
                            canonical.filename().generic_string(),
                            canonicalDriver.string(), resolvedWorking.string(),
                            compileOptions);
                      });
                });
              });
        });
  });
}

Result<void> removeFile(Database &database, std::int64_t id) {
  return execute(database, "DELETE FROM file WHERE id=?", id);
}

Result<void> setFileCompileOptions(Database &database, std::int64_t id,
                                   const std::string &compileOptions) {
  return execute(database,
                 "UPDATE file SET compile_options=?,args_overridden=1 "
                 "WHERE id=?",
                 compileOptions, id);
}

} // namespace facts::catalog
