#include "storage/catalog/Database.h"
#include "storage/FileDatabase.h"

namespace facts::catalog {

Result<Database> open(const std::string &path, bool writable, bool create) {
  try {
    if (create && !std::filesystem::exists(path)) {
      FileDatabase initialize(path);
    }
    return FileDatabase::openReadOnly(path)
        .and_then([&](auto) {
          return Database::open(path, writable ? SQLITE_OPEN_READWRITE
                                               : Database::readOnly)
              .transform_error([](auto error) {
                return "cannot open project configuration: " + error.message();
              });
        })
        .and_then([](Database database) -> Result<Database> {
          return execute(database, "PRAGMA foreign_keys=ON").transform([&] {
            return std::move(database);
          });
        });
  } catch (const std::exception &error) {
    return std::unexpected(error.what());
  }
}

Result<std::filesystem::path> existingDirectory(const std::string &path) {
  std::error_code error;
  const auto canonical = std::filesystem::canonical(path, error);
  if (error || !std::filesystem::is_directory(canonical, error)) {
    return std::unexpected("directory not found: " + path);
  }
  return canonical;
}

Result<void> validateVersion(const std::string &version) {
  if (version.empty())
    return {};
  const auto path = std::filesystem::path(version);
  if (path.is_absolute() || path.has_parent_path() || version == "." ||
      version == "..") {
    return std::unexpected("version must be one relative path segment");
  }
  return {};
}
} // namespace facts::catalog
