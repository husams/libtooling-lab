#include "tooling/StoredCompilationReader.h"

#include "storage/ProjectConfiguration.h"
#include "storage/Sqlite.h"
#include "tooling/CompilationCommandCodec.h"

#include <sqlite3.h>

#include <optional>
#include <set>
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

StoredCompileFile storedCompileFile(sqlite3_stmt *statement) {
  auto root = componentRoot(statement);
  return {root,
          (root / storage::columnText(statement, 6) /
           storage::columnText(statement, 7))
              .lexically_normal(),
          storage::columnText(statement, 1),
          storage::columnText(statement, 8),
          storage::columnText(statement, 9),
          storage::columnText(statement, 10)};
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
          files.push_back(storedCompileFile(statement.get()));
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

struct RequestedFileIdentity {
  std::int64_t componentId;
  std::filesystem::path path;
  std::string directory;
  std::string name;
};

struct OwningComponent {
  const StoredCompilationComponent *component;
  std::filesystem::path identity;
};

bool ownsPath(const std::filesystem::path &root,
              const std::filesystem::path &path) {
  const auto relative = path.lexically_relative(root);
  return !relative.empty() && !relative.is_absolute() &&
         *relative.begin() != "..";
}

std::expected<OwningComponent, std::string>
selectOwningComponent(const std::vector<StoredCompilationComponent> &components,
                      const std::filesystem::path &logicalPath,
                      const std::filesystem::path &canonicalPath) {
  const StoredCompilationComponent *selected = nullptr;
  std::filesystem::path selectedIdentity;
  std::size_t selectedRootSize = 0;
  bool ambiguous = false;
  bool selectedLogicalOwner = false;
  for (const auto &component : components) {
    const auto root = logicalCompilationPath(component.root);
    const auto logicalOwner = ownsPath(root, logicalPath);
    const auto identity = logicalOwner                    ? &logicalPath
                          : ownsPath(root, canonicalPath) ? &canonicalPath
                                                          : nullptr;
    if (!identity) {
      continue;
    }
    if (logicalOwner && !selectedLogicalOwner) {
      selected = nullptr;
      selectedRootSize = 0;
      ambiguous = false;
      selectedLogicalOwner = true;
    } else if (!logicalOwner && selectedLogicalOwner) {
      continue;
    }
    const auto rootSize = root.native().size();
    if (!selected || rootSize > selectedRootSize) {
      selected = &component;
      selectedIdentity = *identity;
      selectedRootSize = rootSize;
      ambiguous = false;
    } else if (rootSize == selectedRootSize) {
      ambiguous = true;
    }
  }
  if (!selected) {
    return std::unexpected("no stored compile command for requested source '" +
                           logicalPath.string() + "'");
  }
  if (ambiguous) {
    return std::unexpected(
        "ambiguous stored compile commands for requested source '" +
        logicalPath.string() + "'");
  }
  return OwningComponent{selected, std::move(selectedIdentity)};
}

std::expected<RequestedFileIdentity, std::string>
identifyRequestedFile(const std::vector<StoredCompilationComponent> &components,
                      const std::string &requestedSource) {
  const auto logicalPath = logicalCompilationPath(requestedSource);
  const auto owner = selectOwningComponent(
      components, logicalPath, normalizeCompilationPath(logicalPath));
  if (!owner) {
    return std::unexpected(owner.error());
  }
  const auto relative = owner->identity.lexically_relative(
      logicalCompilationPath(owner->component->root));
  return RequestedFileIdentity{owner->component->id, logicalPath,
                               relative.parent_path().generic_string(),
                               relative.filename().generic_string()};
}

std::expected<std::vector<RequestedFileIdentity>, std::string>
identifyRequestedFiles(
    const std::vector<StoredCompilationComponent> &components,
    std::span<const std::string> requestedSources) {
  std::vector<RequestedFileIdentity> identities;
  std::set<std::string> seen;
  for (const auto &source : requestedSources) {
    auto identity = identifyRequestedFile(components, source);
    if (!identity) {
      return std::unexpected(identity.error());
    }
    if (seen.insert(identity->path.string()).second) {
      identities.push_back(std::move(*identity));
    }
  }
  return identities;
}

std::expected<void, std::string>
bindRequestedFile(storage::Statement &statement,
                  const RequestedFileIdentity &identity, sqlite3 *database) {
  const auto directory = identity.directory;
  const auto name = identity.name;
  if (sqlite3_reset(statement.get()) != SQLITE_OK ||
      sqlite3_clear_bindings(statement.get()) != SQLITE_OK ||
      sqlite3_bind_int64(statement.get(), 1, identity.componentId) !=
          SQLITE_OK ||
      sqlite3_bind_text(statement.get(), 2, directory.c_str(), -1,
                        SQLITE_TRANSIENT) != SQLITE_OK ||
      sqlite3_bind_text(statement.get(), 3, name.c_str(), -1,
                        SQLITE_TRANSIENT) != SQLITE_OK) {
    return std::unexpected(sqliteMessage(database));
  }
  return {};
}

std::expected<StoredCompileFile, std::string>
readRequestedFile(storage::Statement &statement,
                  const RequestedFileIdentity &identity, sqlite3 *database) {
  return bindRequestedFile(statement, identity, database)
      .and_then([&]() -> std::expected<StoredCompileFile, std::string> {
        auto status = sqlite3_step(statement.get());
        if (status == SQLITE_DONE)
          return std::unexpected("requested source is not imported: '" +
                                 identity.path.string() + "'");
        if (status != SQLITE_ROW)
          return std::unexpected(sqliteMessage(database));
        if (sqlite3_column_type(statement.get(), 10) == SQLITE_NULL)
          return std::unexpected(
              "no stored compile command for requested source '" +
              identity.path.string() + "'");
        auto file = storedCompileFile(statement.get());
        status = sqlite3_step(statement.get());
        for (; status == SQLITE_ROW; status = sqlite3_step(statement.get()))
          if (sqlite3_column_type(statement.get(), 10) != SQLITE_NULL)
            return std::unexpected(
                "ambiguous stored compile commands for requested source '" +
                identity.path.string() + "'");
        if (status != SQLITE_DONE)
          return std::unexpected(sqliteMessage(database));
        return file;
      });
}

std::expected<std::vector<StoredCompileFile>, std::string>
readRequestedFiles(sqlite3 *database,
                   const std::vector<RequestedFileIdentity> &identities) {
  constexpr auto sql =
      "SELECT component.id, component.name, component.path, "
      "component.version, component.repository_id, clone.path, "
      "directory.path, file.name, file.driver, file.working_directory, "
      "file.compile_options "
      "FROM file JOIN directory ON directory.id=file.directory_id "
      "JOIN component ON component.id=directory.component_id "
      "LEFT JOIN repository ON repository.id=component.repository_id "
      "LEFT JOIN clone ON clone.id=repository.active_clone_id "
      "WHERE component.id=?1 AND directory.path=?2 AND file.name=?3 "
      "ORDER BY file.compile_options IS NULL,file.id";
  return prepareStatement(database, sql)
      .and_then([database, &identities](storage::Statement statement) {
        std::vector<StoredCompileFile> files;
        files.reserve(identities.size());
        for (const auto &identity : identities) {
          auto file = readRequestedFile(statement, identity, database);
          if (!file) {
            return std::expected<std::vector<StoredCompileFile>, std::string>{
                std::unexpected(file.error())};
          }
          files.push_back(std::move(*file));
        }
        return std::expected<std::vector<StoredCompileFile>, std::string>{
            std::move(files)};
      });
}

std::expected<std::vector<StoredCompileFile>, std::string>
readSelectedFiles(sqlite3 *database,
                  const std::vector<StoredCompilationComponent> &components,
                  std::span<const std::string> requestedSources) {
  return requestedSources.empty()
             ? readStoredFiles(database)
             : identifyRequestedFiles(components, requestedSources)
                   .and_then([database](const auto &identities) {
                     return readRequestedFiles(database, identities);
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
    return std::unexpected("project configuration database not found: " +
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
readStoredCompilation(sqlite3 *database,
                      std::span<const std::string> requestedSources) {
  return readComponents(database).and_then(
      [database,
       requestedSources](std::vector<StoredCompilationComponent> components) {
        return readLabels(database).and_then(
            [database, requestedSources, components = std::move(components)](
                StoredCommandAliases labels) mutable {
              return readSelectedFiles(database, components, requestedSources)
                  .transform([components = std::move(components),
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
