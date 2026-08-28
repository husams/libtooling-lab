#include "tooling/StoredCompilationReader.h"

#include "storage/ProjectConfiguration.h"
#include "storage/Sqlite.h"

#include <sqlite3.h>

#include <optional>
#include <string_view>
#include <utility>

namespace facts {
namespace {

std::string sqliteMessage(sqlite3 *database) {
  return database ? sqlite3_errmsg(database) : "cannot open SQLite database";
}

std::expected<storage::Statement, std::string>
prepareStatement(sqlite3 *database, std::string_view sql) {
  return storage::prepare(database, sql)
      .transform_error([database](const std::error_code &) {
        return sqliteMessage(database);
      });
}

std::filesystem::path componentRoot(sqlite3_stmt *statement) {
  ProjectComponent component;
  component.name = storage::columnText(statement, 1);
  component.path = storage::columnText(statement, 2);
  if (const auto version = storage::columnText(statement, 3);
      !version.empty()) {
    component.version = version;
  }
  if (sqlite3_column_type(statement, 4) != SQLITE_NULL) {
    component.repositoryId = sqlite3_column_int64(statement, 4);
  }
  const auto clonePath = storage::columnText(statement, 5);
  const auto clone =
      clonePath.empty()
          ? std::optional<ProjectClone>{}
          : std::optional<ProjectClone>{ProjectClone{.path = clonePath}};
  return effectiveComponentRoot(component, clone);
}

std::expected<std::vector<StoredCompilationComponent>, std::string>
readComponents(sqlite3 *database) {
  constexpr auto sql =
      "SELECT component.id, component.name, component.path, "
      "component.version, component.repository_id, clone.path "
      "FROM component "
      "LEFT JOIN repository ON repository.id=component.repository_id "
      "LEFT JOIN clone ON clone.id=repository.active_clone_id "
      "ORDER BY component.id";
  return prepareStatement(database, sql)
      .and_then([database](storage::Statement statement) {
        std::vector<StoredCompilationComponent> components;
        auto status = sqlite3_step(statement.get());
        while (status == SQLITE_ROW) {
          components.push_back({sqlite3_column_int64(statement.get(), 0),
                                storage::columnText(statement.get(), 1),
                                componentRoot(statement.get())});
          status = sqlite3_step(statement.get());
        }
        if (status != SQLITE_DONE) {
          return std::expected<std::vector<StoredCompilationComponent>,
                               std::string>{
              std::unexpected(sqliteMessage(database))};
        }
        return std::expected<std::vector<StoredCompilationComponent>,
                             std::string>{std::move(components)};
      });
}

std::expected<std::vector<StoredCompileFile>, std::string>
readStoredFiles(sqlite3 *database) {
  constexpr auto sql =
      "SELECT component.id, component.name, component.path, "
      "component.version, component.repository_id, clone.path, "
      "directory.path, file.name, file.driver, file.working_directory, "
      "file.compile_options "
      "FROM file JOIN directory ON directory.id=file.directory_id "
      "JOIN component ON component.id=directory.component_id "
      "LEFT JOIN repository ON repository.id=component.repository_id "
      "LEFT JOIN clone ON clone.id=repository.active_clone_id "
      "WHERE file.compile_options IS NOT NULL ORDER BY file.id";
  return prepareStatement(database, sql)
      .and_then([database](storage::Statement statement) {
        std::vector<StoredCompileFile> files;
        auto status = sqlite3_step(statement.get());
        while (status == SQLITE_ROW) {
          auto root = componentRoot(statement.get());
          files.push_back({root,
                           (root / storage::columnText(statement.get(), 6) /
                            storage::columnText(statement.get(), 7))
                               .lexically_normal(),
                           storage::columnText(statement.get(), 1),
                           storage::columnText(statement.get(), 8),
                           storage::columnText(statement.get(), 9),
                           storage::columnText(statement.get(), 10)});
          status = sqlite3_step(statement.get());
        }
        if (status != SQLITE_DONE) {
          return std::expected<std::vector<StoredCompileFile>, std::string>{
              std::unexpected(sqliteMessage(database))};
        }
        return std::expected<std::vector<StoredCompileFile>, std::string>{
            std::move(files)};
      });
}

bool hasTable(sqlite3 *database, std::string_view name) {
  constexpr auto sql =
      "SELECT EXISTS(SELECT 1 FROM sqlite_master WHERE type='table' "
      "AND name=?1)";
  auto statement = prepareStatement(database, sql);
  if (!statement ||
      sqlite3_bind_text(statement->get(), 1, name.data(),
                        static_cast<int>(name.size()),
                        SQLITE_TRANSIENT) != SQLITE_OK ||
      sqlite3_step(statement->get()) != SQLITE_ROW) {
    return false;
  }
  return sqlite3_column_int(statement->get(), 0) != 0;
}

std::expected<StoredCommandAliases, std::string> readLabels(sqlite3 *database) {
  if (!hasTable(database, "label")) {
    return StoredCommandAliases{};
  }
  return prepareStatement(database, "SELECT name,path FROM label ORDER BY name")
      .and_then([database](storage::Statement statement) {
        StoredCommandAliases labels;
        auto status = sqlite3_step(statement.get());
        while (status == SQLITE_ROW) {
          labels.insert_or_assign(storage::columnText(statement.get(), 0),
                                  storage::columnText(statement.get(), 1));
          status = sqlite3_step(statement.get());
        }
        if (status != SQLITE_DONE) {
          return std::expected<StoredCommandAliases, std::string>{
              std::unexpected(sqliteMessage(database))};
        }
        return std::expected<StoredCommandAliases, std::string>{
            std::move(labels)};
      });
}

} // namespace

StoredDatabase::StoredDatabase(sqlite3 *handle) noexcept : handle_(handle) {}

StoredDatabase::StoredDatabase(StoredDatabase &&other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)) {}

StoredDatabase &StoredDatabase::operator=(StoredDatabase &&other) noexcept {
  if (handle_) {
    sqlite3_close(handle_);
  }
  handle_ = std::exchange(other.handle_, nullptr);
  return *this;
}

StoredDatabase::~StoredDatabase() {
  if (handle_) {
    sqlite3_close(handle_);
  }
}

sqlite3 *StoredDatabase::get() const noexcept { return handle_; }

std::expected<StoredDatabase, std::string>
openStoredDatabase(const std::filesystem::path &path) {
  if (!std::filesystem::is_regular_file(path)) {
    return std::unexpected("stored compilation database does not exist: " +
                           path.string());
  }
  sqlite3 *database = nullptr;
  if (sqlite3_open_v2(path.c_str(), &database, SQLITE_OPEN_READONLY, nullptr) !=
      SQLITE_OK) {
    auto error = sqliteMessage(database);
    if (database) {
      sqlite3_close(database);
    }
    return std::unexpected(std::move(error));
  }
  return StoredDatabase(database);
}

std::expected<StoredCompilationSnapshot, std::string>
readStoredCompilation(sqlite3 *database) {
  return readComponents(database).and_then(
      [database](std::vector<StoredCompilationComponent> components) {
        return readLabels(database).and_then(
            [database, components = std::move(components)](
                StoredCommandAliases labels) mutable {
              return readStoredFiles(database).transform(
                  [components = std::move(components),
                   labels = std::move(labels)](
                      std::vector<StoredCompileFile> files) mutable {
                    return StoredCompilationSnapshot{
                        .components = std::move(components),
                        .files = std::move(files),
                        .labels = std::move(labels)};
                  });
            });
      });
}

} // namespace facts
