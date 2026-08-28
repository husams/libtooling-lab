#include "storage/FileIdentity.h"

#include "storage/Sqlite.h"

#include <sqlite3.h>

#include <optional>
#include <utility>

namespace facts {
namespace {

std::filesystem::path normalizedSource(std::filesystem::path source) {
  if (source.is_absolute()) {
    return source.lexically_normal();
  }
  auto absolute = std::filesystem::absolute(source).lexically_normal();
  std::error_code error;
  auto canonical = std::filesystem::weakly_canonical(absolute, error);
  return error ? absolute : canonical;
}

ProjectComponent readComponent(sqlite3_stmt *statement) {
  ProjectComponent component;
  component.id = sqlite3_column_int64(statement, 0);
  component.name = storage::columnText(statement, 1);
  component.path = storage::columnText(statement, 2);
  component.kind = storage::columnText(statement, 3);
  if (const auto version = storage::columnText(statement, 4);
      !version.empty()) {
    component.version = version;
  }
  if (sqlite3_column_type(statement, 5) != SQLITE_NULL) {
    component.repositoryId = sqlite3_column_int64(statement, 5);
  }
  return component;
}

std::optional<ProjectClone> readClone(sqlite3_stmt *statement) {
  if (sqlite3_column_type(statement, 6) == SQLITE_NULL) {
    return std::nullopt;
  }
  return ProjectClone{
      .id = sqlite3_column_int64(statement, 6),
      .repositoryId = sqlite3_column_int64(statement, 7),
      .path = storage::columnText(statement, 8),
      .label = storage::columnText(statement, 9),
  };
}

ProjectComponent materializeComponentRoot(sqlite3_stmt *statement) {
  auto component = readComponent(statement);
  component.path = effectiveComponentRoot(component, readClone(statement));
  component.version.reset();
  component.repositoryId.reset();
  return component;
}

std::error_code outsideComponentError() {
  return std::make_error_code(std::errc::no_such_file_or_directory);
}

} // namespace

std::expected<FileIdentityContext, std::error_code>
loadFileIdentityContext(sqlite3 *database) {
  constexpr std::string_view sql =
      "SELECT component.id, component.name, component.path, component.kind, "
      "component.version, component.repository_id, clone.id, "
      "clone.repository_id, clone.path, clone.label "
      "FROM component "
      "LEFT JOIN repository ON repository.id=component.repository_id "
      "LEFT JOIN clone ON clone.id=repository.active_clone_id "
      "ORDER BY component.id";
  return storage::prepare(database, sql)
      .and_then([&](storage::Statement statement)
                    -> std::expected<FileIdentityContext, std::error_code> {
        FileIdentityContext context;
        auto status = sqlite3_step(statement.get());
        while (status == SQLITE_ROW) {
          context.components.push_back(
              materializeComponentRoot(statement.get()));
          status = sqlite3_step(statement.get());
        }
        if (status != SQLITE_DONE) {
          return std::unexpected(storage::sqliteError(database));
        }
        return context;
      });
}

std::expected<FileIdentity, std::error_code>
identifyFile(std::span<const ProjectComponent> components,
             const ProjectClone &clone, std::filesystem::path source) {
  const auto owner = selectOwningComponent(components, clone, source);
  if (!owner) {
    return std::unexpected(outsideComponentError());
  }

  const auto root = effectiveComponentRoot(components[*owner], clone);
  const auto relative =
      normalizedSource(std::move(source)).lexically_relative(root);
  auto name = relative.filename().string();
  if (relative.empty() || name.empty()) {
    return std::unexpected(outsideComponentError());
  }
  auto directory = relative.parent_path().generic_string();
  if (directory == ".") {
    directory.clear();
  }
  return FileIdentity{components[*owner].id, std::move(directory),
                      std::move(name)};
}

std::expected<std::vector<FileIdentity>, std::error_code>
identifyFiles(const FileIdentityContext &context,
              std::span<const std::string> sources) {
  std::vector<FileIdentity> files;
  files.reserve(sources.size());
  for (const auto &source : sources) {
    auto file = identifyFile(context.components, context.clone, source);
    if (!file && file.error() != outsideComponentError()) {
      return std::unexpected(file.error());
    }
    if (file) {
      files.push_back(std::move(*file));
    }
  }
  return files;
}

} // namespace facts
