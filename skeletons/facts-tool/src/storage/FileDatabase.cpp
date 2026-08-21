#include "storage/FileDatabase.h"

#include "storage/FileSchema.h"

#include <sqlite3.h>

#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace facts {
namespace {

using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

struct FileIdentity {
  std::int64_t componentId;
  std::string directory;
  std::string name;
};

struct ComponentRoot {
  std::int64_t id;
  std::filesystem::path path;
};

std::error_code sqliteError(sqlite3 *database) {
  return {sqlite3_extended_errcode(database), std::generic_category()};
}

std::expected<void, std::error_code> execute(sqlite3 *database,
                                             const char *sql) {
  if (sqlite3_exec(database, sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
    return std::unexpected(sqliteError(database));
  }
  return {};
}

std::expected<Statement, std::error_code> prepare(sqlite3 *database,
                                                  const char *sql) {
  sqlite3_stmt *raw = nullptr;
  if (sqlite3_prepare_v2(database, sql, -1, &raw, nullptr) != SQLITE_OK) {
    return std::unexpected(sqliteError(database));
  }
  return Statement(raw, sqlite3_finalize);
}

std::expected<void, std::error_code> bindText(sqlite3 *database,
                                              sqlite3_stmt *statement,
                                              int position,
                                              std::string_view value) {
  if (sqlite3_bind_text(statement, position, value.data(),
                        static_cast<int>(value.size()),
                        SQLITE_TRANSIENT) != SQLITE_OK) {
    return std::unexpected(sqliteError(database));
  }
  return {};
}

std::expected<std::int64_t, std::error_code>
readRowId(sqlite3 *database, sqlite3_stmt *statement) {
  if (sqlite3_step(statement) != SQLITE_ROW) {
    return std::unexpected(
        std::make_error_code(std::errc::no_such_file_or_directory));
  }
  const auto raw = sqlite3_column_int64(statement, 0);
  if (raw < 1) {
    return std::unexpected(std::make_error_code(std::errc::value_too_large));
  }
  return raw;
}

std::expected<FileId, std::error_code> readFileId(sqlite3 *database,
                                                  sqlite3_stmt *statement) {
  return readRowId(database, statement).and_then([](std::int64_t raw) {
    if (raw < firstPhysicalFileId || raw > std::numeric_limits<FileId>::max()) {
      return std::expected<FileId, std::error_code>{
          std::unexpected(std::make_error_code(std::errc::value_too_large))};
    }
    return std::expected<FileId, std::error_code>{static_cast<FileId>(raw)};
  });
}

std::string columnText(sqlite3_stmt *statement, int column) {
  const auto *value = sqlite3_column_text(statement, column);
  return value ? reinterpret_cast<const char *>(value) : std::string{};
}

std::filesystem::path absolutePath(std::filesystem::path path) {
  auto absolute =
      (path.is_absolute() ? std::move(path) : std::filesystem::absolute(path))
          .lexically_normal();
  std::error_code error;
  auto canonical = std::filesystem::weakly_canonical(absolute, error);
  return error ? absolute : canonical;
}

ComponentRoot componentRoot(sqlite3_stmt *statement) {
  const auto id = sqlite3_column_int64(statement, 0);
  ProjectComponent component;
  component.id = id;
  component.name = columnText(statement, 1);
  component.path = columnText(statement, 2);
  if (const auto version = columnText(statement, 3); !version.empty()) {
    component.version = version;
  }
  if (sqlite3_column_type(statement, 4) != SQLITE_NULL) {
    component.repositoryId = sqlite3_column_int64(statement, 4);
  }
  const auto clonePath = columnText(statement, 5);
  const auto clone =
      clonePath.empty()
          ? std::optional<ProjectClone>{}
          : std::optional<ProjectClone>{ProjectClone{.path = clonePath}};
  return {id, effectiveComponentRoot(component, clone)};
}

std::expected<std::vector<ComponentRoot>, std::error_code>
componentRoots(sqlite3 *database) {
  constexpr auto sql =
      "SELECT component.id, component.name, component.path, "
      "component.version, component.repository_id, clone.path "
      "FROM component "
      "LEFT JOIN repository ON repository.id=component.repository_id "
      "LEFT JOIN clone ON clone.id=repository.active_clone_id";
  return prepare(database, sql).and_then([&](Statement statement) {
    std::vector<ComponentRoot> roots;
    auto status = sqlite3_step(statement.get());
    while (status == SQLITE_ROW) {
      roots.push_back(componentRoot(statement.get()));
      status = sqlite3_step(statement.get());
    }
    if (status != SQLITE_DONE) {
      return std::expected<std::vector<ComponentRoot>, std::error_code>{
          std::unexpected(sqliteError(database))};
    }
    return std::expected<std::vector<ComponentRoot>, std::error_code>{
        std::move(roots)};
  });
}

std::string ownershipKey(const std::filesystem::path &path) {
  auto key = path.string();
  while (!key.empty() && key.back() == '/') {
    key.pop_back();
  }
  return key;
}

bool ownsPath(const ComponentRoot &root, std::string_view identity) {
  const auto key = ownershipKey(root.path);
  return identity == key || identity.starts_with(key + "/");
}

std::optional<FileIdentity> selectIdentity(std::vector<ComponentRoot> roots,
                                           std::string_view identity) {
  std::optional<ComponentRoot> owner;
  for (auto &root : roots) {
    if (ownsPath(root, identity) &&
        (!owner ||
         ownershipKey(root.path).size() > ownershipKey(owner->path).size())) {
      owner = std::move(root);
    }
  }
  if (!owner) {
    return std::nullopt;
  }

  const auto relative =
      std::filesystem::path(identity).lexically_relative(owner->path);
  auto name = relative.filename().string();
  if (relative.empty() || name.empty()) {
    return std::nullopt;
  }
  auto directory = relative.parent_path().generic_string();
  if (directory == ".") {
    directory.clear();
  }
  return FileIdentity{owner->id, std::move(directory), std::move(name)};
}

std::expected<std::optional<FileIdentity>, std::error_code>
splitIdentity(sqlite3 *database, std::string_view identity) {
  return componentRoots(database).transform(
      [&](std::vector<ComponentRoot> roots) {
        return selectIdentity(std::move(roots), identity);
      });
}

std::expected<std::int64_t, std::error_code>
upsertDirectory(sqlite3 *database, std::int64_t componentId,
                std::string_view directory) {
  constexpr auto sql =
      "INSERT INTO directory(component_id, path) VALUES(?1, ?2) "
      "ON CONFLICT(component_id, path) DO UPDATE SET path=excluded.path "
      "RETURNING id";
  return prepare(database, sql).and_then([&](Statement statement) {
    if (sqlite3_bind_int64(statement.get(), 1, componentId) != SQLITE_OK) {
      return std::expected<std::int64_t, std::error_code>{
          std::unexpected(sqliteError(database))};
    }
    return bindText(database, statement.get(), 2, directory).and_then([&] {
      return readRowId(database, statement.get());
    });
  });
}

std::expected<FileId, std::error_code> upsertFile(sqlite3 *database,
                                                  const FileIdentity &file) {
  return upsertDirectory(database, file.componentId, file.directory)
      .and_then([&](std::int64_t directoryId) {
        constexpr auto sql =
            "INSERT INTO file(directory_id, name) VALUES(?1, ?2) "
            "ON CONFLICT(directory_id, name) DO UPDATE SET "
            "name=excluded.name RETURNING id";
        return prepare(database, sql).and_then([&](Statement statement) {
          if (sqlite3_bind_int64(statement.get(), 1, directoryId) !=
              SQLITE_OK) {
            return std::expected<FileId, std::error_code>{
                std::unexpected(sqliteError(database))};
          }
          return bindText(database, statement.get(), 2, file.name)
              .and_then([&] { return readFileId(database, statement.get()); });
        });
      });
}

std::expected<FileId, std::error_code> selectId(sqlite3 *database,
                                                const FileIdentity &file) {
  constexpr auto sql = "SELECT file.id FROM file "
                       "JOIN directory ON directory.id=file.directory_id "
                       "WHERE directory.component_id=?1 "
                       "AND directory.path=?2 AND file.name=?3";
  return prepare(database, sql).and_then([&](Statement statement) {
    if (sqlite3_bind_int64(statement.get(), 1, file.componentId) != SQLITE_OK) {
      return std::expected<FileId, std::error_code>{
          std::unexpected(sqliteError(database))};
    }
    return bindText(database, statement.get(), 2, file.directory)
        .and_then(
            [&] { return bindText(database, statement.get(), 3, file.name); })
        .and_then([&] { return readFileId(database, statement.get()); });
  });
}

std::expected<std::int64_t, std::error_code>
upsertRepository(sqlite3 *database, const ProjectConfiguration &configuration) {
  constexpr auto sql =
      "INSERT INTO repository(name, remote_url) VALUES(?1, ?2) "
      "ON CONFLICT(name) DO UPDATE SET remote_url=excluded.remote_url "
      "RETURNING id";
  return prepare(database, sql).and_then([&](Statement statement) {
    return bindText(database, statement.get(), 1, configuration.repositoryName)
        .and_then([&] {
          return bindText(database, statement.get(), 2,
                          configuration.remoteUrl);
        })
        .and_then([&] { return readRowId(database, statement.get()); });
  });
}

std::expected<std::int64_t, std::error_code>
upsertClone(sqlite3 *database, std::int64_t repositoryId,
            const ProjectClone &clone) {
  constexpr auto sql =
      "INSERT INTO clone(repository_id, path, label) VALUES(?1, ?2, ?3) "
      "ON CONFLICT(path) DO UPDATE SET label=excluded.label "
      "RETURNING id";
  return prepare(database, sql).and_then([&](Statement statement) {
    if (sqlite3_bind_int64(statement.get(), 1, repositoryId) != SQLITE_OK) {
      return std::expected<std::int64_t, std::error_code>{
          std::unexpected(sqliteError(database))};
    }
    return bindText(database, statement.get(), 2, clone.path)
        .and_then(
            [&] { return bindText(database, statement.get(), 3, clone.label); })
        .and_then([&] { return readRowId(database, statement.get()); });
  });
}

std::expected<void, std::error_code> setActiveClone(sqlite3 *database,
                                                    std::int64_t repositoryId,
                                                    std::int64_t cloneId) {
  constexpr auto sql = "UPDATE repository SET active_clone_id=?1 WHERE id=?2";
  return prepare(database, sql).and_then([&](Statement statement) {
    if (sqlite3_bind_int64(statement.get(), 1, cloneId) != SQLITE_OK ||
        sqlite3_bind_int64(statement.get(), 2, repositoryId) != SQLITE_OK) {
      return std::expected<void, std::error_code>{
          std::unexpected(sqliteError(database))};
    }
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
      return std::expected<void, std::error_code>{
          std::unexpected(sqliteError(database))};
    }
    return std::expected<void, std::error_code>{};
  });
}

std::expected<std::int64_t, std::error_code>
upsertComponent(sqlite3 *database, std::int64_t repositoryId,
                const ProjectComponent &component) {
  constexpr auto sql =
      "INSERT INTO component(name, path, kind, version, repository_id, "
      "semantic_universe_id) VALUES(?1, ?2, ?3, ?4, ?5, 1) "
      "ON CONFLICT(repository_id, path) DO UPDATE SET "
      "name=excluded.name, kind=excluded.kind, version=excluded.version "
      "RETURNING id";
  return prepare(database, sql).and_then([&](Statement statement) {
    if (sqlite3_bind_int64(statement.get(), 5, repositoryId) != SQLITE_OK) {
      return std::expected<std::int64_t, std::error_code>{
          std::unexpected(sqliteError(database))};
    }
    return bindText(database, statement.get(), 1, component.name)
        .and_then([&] {
          return bindText(database, statement.get(), 2, component.path);
        })
        .and_then([&] {
          return bindText(database, statement.get(), 3, component.kind);
        })
        .and_then([&] {
          if (component.version) {
            return bindText(database, statement.get(), 4, *component.version);
          }
          return sqlite3_bind_null(statement.get(), 4) == SQLITE_OK
                     ? std::expected<void, std::error_code>{}
                     : std::expected<void, std::error_code>{
                           std::unexpected(sqliteError(database))};
        })
        .and_then([&] { return readRowId(database, statement.get()); });
  });
}

std::expected<void, std::error_code>
clearCompileConfigurations(sqlite3 *database) {
  return execute(database, "UPDATE file SET compile_options=NULL, driver=NULL, "
                           "working_directory=NULL");
}

std::expected<std::int64_t, std::error_code>
upsertProjectFile(sqlite3 *database, std::int64_t componentId,
                  const ProjectFile &file) {
  return upsertDirectory(database, componentId, file.directory)
      .and_then([&](std::int64_t directoryId) {
        constexpr auto sql =
            "INSERT INTO file(directory_id, name, driver, working_directory, "
            "compile_options) VALUES(?1, ?2, ?3, ?4, ?5) "
            "ON CONFLICT(directory_id, name) DO UPDATE SET "
            "driver=excluded.driver, "
            "working_directory=excluded.working_directory, "
            "compile_options=excluded.compile_options RETURNING id";
        return prepare(database, sql).and_then([&](Statement statement) {
          if (sqlite3_bind_int64(statement.get(), 1, directoryId) !=
              SQLITE_OK) {
            return std::expected<std::int64_t, std::error_code>{
                std::unexpected(sqliteError(database))};
          }
          return bindText(database, statement.get(), 2, file.name)
              .and_then([&] {
                return bindText(database, statement.get(), 3, file.driver);
              })
              .and_then([&] {
                if (!file.workingDirectory.empty()) {
                  return bindText(database, statement.get(), 4,
                                  file.workingDirectory);
                }
                return sqlite3_bind_null(statement.get(), 4) == SQLITE_OK
                           ? std::expected<void, std::error_code>{}
                           : std::expected<void, std::error_code>{
                                 std::unexpected(sqliteError(database))};
              })
              .and_then([&] {
                return bindText(database, statement.get(), 5,
                                file.compileOptions);
              })
              .and_then([&] { return readRowId(database, statement.get()); });
        });
      });
}

template <std::ranges::input_range Results>
auto collect(Results &&results)
    -> std::expected<void,
                     typename std::ranges::range_value_t<Results>::error_type> {
  using Result = std::ranges::range_value_t<Results>;
  using Error = typename Result::error_type;
  for (auto result : results) {
    if (!result) {
      return std::unexpected<Error>(result.error());
    }
  }
  return std::expected<void, Error>{};
}

template <typename Work>
auto inImmediateTransaction(sqlite3 *database, Work work)
    -> std::invoke_result_t<Work> {
  auto started = execute(database, "BEGIN IMMEDIATE");
  if (!started) {
    return std::unexpected(started.error());
  }

  auto result = std::invoke(std::move(work));
  if (!result) {
    execute(database, "ROLLBACK");
    return result;
  }

  auto committed = execute(database, "COMMIT");
  if (!committed) {
    execute(database, "ROLLBACK");
    return std::unexpected(committed.error());
  }
  return result;
}

std::expected<bool, std::error_code> usesLegacyFileSchema(sqlite3 *database) {
  constexpr auto sql = "SELECT EXISTS(SELECT 1 FROM pragma_table_info('file') "
                       "WHERE name='path')";
  return prepare(database, sql).and_then([&](Statement statement) {
    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
      return std::expected<bool, std::error_code>{
          std::unexpected(sqliteError(database))};
    }
    return std::expected<bool, std::error_code>{
        sqlite3_column_int(statement.get(), 0) != 0};
  });
}

std::expected<bool, std::error_code> hasFileColumn(sqlite3 *database,
                                                   std::string_view name) {
  constexpr auto sql =
      "SELECT EXISTS(SELECT 1 FROM pragma_table_info('file') WHERE name=?1)";
  return prepare(database, sql).and_then([&](Statement statement) {
    return bindText(database, statement.get(), 1, name).and_then([&] {
      if (sqlite3_step(statement.get()) != SQLITE_ROW) {
        return std::expected<bool, std::error_code>{
            std::unexpected(sqliteError(database))};
      }
      return std::expected<bool, std::error_code>{
          sqlite3_column_int(statement.get(), 0) != 0};
    });
  });
}

std::expected<void, std::error_code>
ensureProjectConfigurationSchema(sqlite3 *database) {
  return hasFileColumn(database, "working_directory")
      .and_then([database](bool present) {
        return present
                   ? std::expected<void, std::error_code>{}
                   : execute(database,
                             "ALTER TABLE file ADD COLUMN working_directory "
                             "TEXT");
      });
}

std::expected<bool, std::error_code>
usesFlatDirectorySchema(sqlite3 *database) {
  constexpr auto sql =
      "SELECT EXISTS(SELECT 1 FROM pragma_table_info('directory') "
      "WHERE name='path') AND NOT EXISTS(SELECT 1 FROM "
      "pragma_table_info('directory') WHERE name='component_id')";
  return prepare(database, sql).and_then([&](Statement statement) {
    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
      return std::expected<bool, std::error_code>{
          std::unexpected(sqliteError(database))};
    }
    return std::expected<bool, std::error_code>{
        sqlite3_column_int(statement.get(), 0) != 0};
  });
}

std::expected<void, std::error_code> ensureDefaultComponent(sqlite3 *database) {
  constexpr auto sql =
      "INSERT INTO component(name, path, kind, semantic_universe_id) "
      "SELECT 'facts-tool', '/', 'external', 1 "
      "WHERE NOT EXISTS(SELECT 1 FROM component)";
  return execute(database, sql);
}

std::expected<void, std::error_code>
insertLegacyFile(sqlite3 *database, FileId id, const FileIdentity &file) {
  return upsertDirectory(database, file.componentId, file.directory)
      .and_then([&](std::int64_t directoryId) {
        constexpr auto sql =
            "INSERT INTO file(id, directory_id, name) VALUES(?1, ?2, ?3)";
        return prepare(database, sql).and_then([&](Statement statement) {
          if (sqlite3_bind_int64(statement.get(), 1, id) != SQLITE_OK ||
              sqlite3_bind_int64(statement.get(), 2, directoryId) !=
                  SQLITE_OK) {
            return std::expected<void, std::error_code>{
                std::unexpected(sqliteError(database))};
          }
          return bindText(database, statement.get(), 3, file.name)
              .and_then([&] {
                if (sqlite3_step(statement.get()) != SQLITE_DONE) {
                  return std::expected<void, std::error_code>{
                      std::unexpected(sqliteError(database))};
                }
                return std::expected<void, std::error_code>{};
              });
        });
      });
}

std::expected<void, std::error_code> migrateLegacyRows(sqlite3 *database) {
  return prepare(database, "SELECT id, path FROM legacy_file ORDER BY id")
      .and_then([&](Statement statement) {
        auto status = sqlite3_step(statement.get());
        while (status == SQLITE_ROW) {
          const auto rawId = sqlite3_column_int64(statement.get(), 0);
          const auto *rawPath = sqlite3_column_text(statement.get(), 1);
          if (rawId < firstPhysicalFileId ||
              rawId > std::numeric_limits<FileId>::max() || !rawPath) {
            return std::expected<void, std::error_code>{std::unexpected(
                std::make_error_code(std::errc::invalid_argument))};
          }
          auto inserted =
              splitIdentity(database, reinterpret_cast<const char *>(rawPath))
                  .and_then([&](const std::optional<FileIdentity> &file) {
                    if (!file) {
                      return std::expected<void, std::error_code>{
                          std::unexpected(std::make_error_code(
                              std::errc::no_such_file_or_directory))};
                    }
                    return insertLegacyFile(database,
                                            static_cast<FileId>(rawId), *file);
                  });
          if (!inserted) {
            return inserted;
          }
          status = sqlite3_step(statement.get());
        }
        if (status != SQLITE_DONE) {
          return std::expected<void, std::error_code>{
              std::unexpected(sqliteError(database))};
        }
        return std::expected<void, std::error_code>{};
      });
}

std::expected<void, std::error_code>
migrateLegacyFileSchema(sqlite3 *database) {
  return execute(database, "ALTER TABLE file RENAME TO legacy_file")
      .and_then([&] { return execute(database, fileSchemaSql); })
      .and_then([&] { return ensureDefaultComponent(database); })
      .and_then([&] { return migrateLegacyRows(database); })
      .and_then([&] { return execute(database, "DROP TABLE legacy_file"); });
}

std::expected<void, std::error_code>
migrateFlatDirectoryRows(sqlite3 *database) {
  constexpr auto sql =
      "SELECT legacy_file.id, legacy_directory.path, legacy_file.name "
      "FROM legacy_file JOIN legacy_directory "
      "ON legacy_directory.id=legacy_file.directory_id "
      "ORDER BY legacy_file.id";
  return prepare(database, sql).and_then([&](Statement statement) {
    auto status = sqlite3_step(statement.get());
    while (status == SQLITE_ROW) {
      const auto rawId = sqlite3_column_int64(statement.get(), 0);
      const auto identity =
          (std::filesystem::path(columnText(statement.get(), 1)) /
           columnText(statement.get(), 2))
              .lexically_normal()
              .string();
      if (rawId < firstPhysicalFileId ||
          rawId > std::numeric_limits<FileId>::max()) {
        return std::expected<void, std::error_code>{
            std::unexpected(std::make_error_code(std::errc::invalid_argument))};
      }
      auto inserted =
          splitIdentity(database, identity)
              .and_then([&](const std::optional<FileIdentity> &file) {
                if (!file) {
                  return std::expected<void, std::error_code>{
                      std::unexpected(std::make_error_code(
                          std::errc::no_such_file_or_directory))};
                }
                return insertLegacyFile(database, static_cast<FileId>(rawId),
                                        *file);
              });
      if (!inserted) {
        return inserted;
      }
      status = sqlite3_step(statement.get());
    }
    if (status != SQLITE_DONE) {
      return std::expected<void, std::error_code>{
          std::unexpected(sqliteError(database))};
    }
    return std::expected<void, std::error_code>{};
  });
}

std::expected<void, std::error_code>
migrateFlatDirectorySchema(sqlite3 *database) {
  return execute(database, "ALTER TABLE file RENAME TO legacy_file")
      .and_then([&] {
        return execute(database,
                       "ALTER TABLE directory RENAME TO legacy_directory");
      })
      .and_then([&] { return execute(database, fileSchemaSql); })
      .and_then([&] { return ensureDefaultComponent(database); })
      .and_then([&] { return migrateFlatDirectoryRows(database); })
      .and_then([&] { return execute(database, "DROP TABLE legacy_file"); })
      .and_then(
          [&] { return execute(database, "DROP TABLE legacy_directory"); });
}

std::expected<void, std::error_code> initializeFileSchema(sqlite3 *database) {
  return inImmediateTransaction(database, [&] {
    return usesLegacyFileSchema(database).and_then([&](bool legacy) {
      if (legacy) {
        return migrateLegacyFileSchema(database);
      }
      return usesFlatDirectorySchema(database).and_then([&](bool flat) {
        if (flat) {
          return migrateFlatDirectorySchema(database);
        }
        return execute(database, fileSchemaSql)
            .and_then(
                [&] { return ensureProjectConfigurationSchema(database); })
            .and_then([&] { return ensureDefaultComponent(database); });
      });
    });
  });
}

} // namespace

struct FileDatabase::Connection {
  explicit Connection(const std::string &path) {
    constexpr int flags =
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2(path.c_str(), &handle, flags, nullptr) != SQLITE_OK) {
      const std::string message =
          handle ? sqlite3_errmsg(handle) : "cannot allocate SQLite handle";
      if (handle) {
        sqlite3_close(handle);
        handle = nullptr;
      }
      throw std::runtime_error(message);
    }
    sqlite3_busy_timeout(handle, 10000);
    const auto initialized =
        execute(handle, "PRAGMA foreign_keys=ON").and_then([&] {
          return initializeFileSchema(handle);
        });
    if (!initialized) {
      throw std::runtime_error(sqlite3_errmsg(handle));
    }
  }

  ~Connection() {
    if (handle) {
      sqlite3_close(handle);
    }
  }

  sqlite3 *handle = nullptr;
};

FileDatabase::FileDatabase(const std::string &path)
    : connection_(std::make_unique<Connection>(path)) {}

FileDatabase::~FileDatabase() = default;

std::expected<void, std::error_code>
FileDatabase::addBulk(std::span<const std::string> identities) {
  return inImmediateTransaction(connection_->handle, [&] {
    auto records =
        identities | std::views::transform([&](const auto &identity) {
          return splitIdentity(connection_->handle, identity)
              .and_then([&](const std::optional<FileIdentity> &file) {
                if (!file) {
                  return std::expected<void, std::error_code>{};
                }
                return upsertFile(connection_->handle, *file)
                    .transform([](FileId) {});
              });
        });
    return collect(records);
  });
}

std::expected<void, std::error_code> FileDatabase::replaceProjectConfiguration(
    const ProjectConfiguration &configuration) {
  if (configuration.repositoryName.empty() ||
      configuration.activeClone.path.empty() ||
      configuration.components.empty()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  return inImmediateTransaction(connection_->handle, [&] {
    return upsertRepository(connection_->handle, configuration)
        .and_then([&](std::int64_t repositoryId) {
          return upsertClone(connection_->handle, repositoryId,
                             configuration.activeClone)
              .and_then([&](std::int64_t cloneId) {
                return setActiveClone(connection_->handle, repositoryId,
                                      cloneId);
              })
              .and_then([&] {
                return clearCompileConfigurations(connection_->handle);
              })
              .and_then([&] {
                std::map<std::string, std::int64_t> componentIds;
                for (const auto &component : configuration.components) {
                  if (component.path.empty() ||
                      componentIds.contains(component.path)) {
                    return std::expected<void, std::error_code>{std::unexpected(
                        std::make_error_code(std::errc::invalid_argument))};
                  }
                  auto saved = upsertComponent(connection_->handle,
                                               repositoryId, component);
                  if (!saved) {
                    return std::expected<void, std::error_code>{
                        std::unexpected(saved.error())};
                  }
                  componentIds.emplace(component.path, *saved);
                }
                for (const auto &file : configuration.files) {
                  const auto component = componentIds.find(file.componentPath);
                  if (component == componentIds.end() || file.name.empty() ||
                      file.compileOptions.empty()) {
                    return std::expected<void, std::error_code>{std::unexpected(
                        std::make_error_code(std::errc::invalid_argument))};
                  }
                  auto saved = upsertProjectFile(connection_->handle,
                                                 component->second, file);
                  if (!saved) {
                    return std::expected<void, std::error_code>{
                        std::unexpected(saved.error())};
                  }
                }
                return std::expected<void, std::error_code>{};
              });
        });
  });
}

std::expected<void, std::error_code>
FileDatabase::switchActiveClone(std::string_view repositoryName,
                                std::string_view clonePathOrLabel) {
  return inImmediateTransaction(connection_->handle, [&] {
    constexpr auto sql =
        "UPDATE repository SET active_clone_id=(SELECT id FROM clone "
        "WHERE repository_id=repository.id AND (path=?2 OR label=?2)) "
        "WHERE name=?1 AND EXISTS(SELECT 1 FROM clone "
        "WHERE repository_id=repository.id AND (path=?2 OR label=?2))";
    return prepare(connection_->handle, sql).and_then([&](Statement statement) {
      return bindText(connection_->handle, statement.get(), 1, repositoryName)
          .and_then([&] {
            return bindText(connection_->handle, statement.get(), 2,
                            clonePathOrLabel);
          })
          .and_then([&] {
            if (sqlite3_step(statement.get()) != SQLITE_DONE) {
              return std::expected<void, std::error_code>{
                  std::unexpected(sqliteError(connection_->handle))};
            }
            return sqlite3_changes(connection_->handle) == 1
                       ? std::expected<void, std::error_code>{}
                       : std::expected<void, std::error_code>{
                             std::unexpected(std::make_error_code(
                                 std::errc::no_such_file_or_directory))};
          });
    });
  });
}

std::expected<void, std::error_code>
FileDatabase::addClone(std::string_view repositoryName,
                       const ProjectClone &clone, bool activate) {
  if (repositoryName.empty() || clone.path.empty()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  return inImmediateTransaction(connection_->handle, [&] {
    constexpr auto sql = "SELECT id FROM repository WHERE name=?1";
    return prepare(connection_->handle, sql).and_then([&](Statement statement) {
      return bindText(connection_->handle, statement.get(), 1, repositoryName)
          .and_then(
              [&] { return readRowId(connection_->handle, statement.get()); })
          .and_then([&](std::int64_t repositoryId) {
            return upsertClone(connection_->handle, repositoryId, clone)
                .and_then([&](std::int64_t cloneId) {
                  return activate ? setActiveClone(connection_->handle,
                                                   repositoryId, cloneId)
                                  : std::expected<void, std::error_code>{};
                });
          });
    });
  });
}

std::expected<FileId, std::error_code>
FileDatabase::getId(std::string_view identity) {
  return splitIdentity(connection_->handle, identity)
      .and_then([&](const std::optional<FileIdentity> &file) {
        if (!file) {
          return std::expected<FileId, std::error_code>{std::unexpected(
              std::make_error_code(std::errc::no_such_file_or_directory))};
        }
        return selectId(connection_->handle, *file);
      });
}

} // namespace facts
