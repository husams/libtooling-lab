#include "storage/FileDatabase.h"

#include "storage/FileSchema.h"
#include "storage/ItlibGenerator.h"
#include "storage/StorageQuery.h"

#include <sqlite3.h>

#include <array>
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

std::error_code legacySqliteError(sqlite3 *database) {
  return {sqlite3_extended_errcode(database), std::generic_category()};
}

std::expected<void, std::error_code> legacyExecute(sqlite3 *database,
                                                   const char *sql) {
  if (sqlite3_exec(database, sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
    return std::unexpected(legacySqliteError(database));
  }
  return {};
}

std::expected<Statement, std::error_code> legacyPrepare(sqlite3 *database,
                                                        const char *sql) {
  sqlite3_stmt *raw = nullptr;
  if (sqlite3_prepare_v2(database, sql, -1, &raw, nullptr) != SQLITE_OK) {
    return std::unexpected(legacySqliteError(database));
  }
  return Statement(raw, sqlite3_finalize);
}

std::expected<void, std::error_code> legacyBindText(sqlite3 *database,
                                                    sqlite3_stmt *statement,
                                                    int position,
                                                    std::string_view value) {
  if (sqlite3_bind_text(statement, position, value.data(),
                        static_cast<int>(value.size()),
                        SQLITE_TRANSIENT) != SQLITE_OK) {
    return std::unexpected(legacySqliteError(database));
  }
  return {};
}

std::expected<std::int64_t, std::error_code>
legacyReadRowId(sqlite3 *database, sqlite3_stmt *statement) {
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

std::string legacyColumnText(sqlite3_stmt *statement, int column) {
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

ComponentRoot legacyComponentRoot(sqlite3_stmt *statement) {
  const auto id = sqlite3_column_int64(statement, 0);
  ProjectComponent component;
  component.id = id;
  component.name = legacyColumnText(statement, 1);
  component.path = legacyColumnText(statement, 2);
  if (const auto version = legacyColumnText(statement, 3); !version.empty()) {
    component.version = version;
  }
  if (sqlite3_column_type(statement, 4) != SQLITE_NULL) {
    component.repositoryId = sqlite3_column_int64(statement, 4);
  }
  const auto clonePath = legacyColumnText(statement, 5);
  const auto clone =
      clonePath.empty()
          ? std::optional<ProjectClone>{}
          : std::optional<ProjectClone>{ProjectClone{.path = clonePath}};
  return {id, effectiveComponentRoot(component, clone)};
}

std::expected<std::vector<ComponentRoot>, std::error_code>
legacyComponentRoots(sqlite3 *database) {
  constexpr auto sql =
      "SELECT component.id, component.name, component.path, "
      "component.version, component.repository_id, clone.path "
      "FROM component "
      "LEFT JOIN repository ON repository.id=component.repository_id "
      "LEFT JOIN clone ON clone.id=repository.active_clone_id";
  return legacyPrepare(database, sql).and_then([&](Statement statement) {
    std::vector<ComponentRoot> roots;
    auto status = sqlite3_step(statement.get());
    while (status == SQLITE_ROW) {
      roots.push_back(legacyComponentRoot(statement.get()));
      status = sqlite3_step(statement.get());
    }
    if (status != SQLITE_DONE) {
      return std::expected<std::vector<ComponentRoot>, std::error_code>{
          std::unexpected(legacySqliteError(database))};
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
legacySplitIdentity(sqlite3 *database, std::string_view identity) {
  return legacyComponentRoots(database).transform(
      [&](std::vector<ComponentRoot> roots) {
        return selectIdentity(std::move(roots), identity);
      });
}

ComponentRoot currentComponentRoot(const storage::Row &row) {
  ProjectComponent component;
  component.id = row.get<std::int64_t>(0);
  component.name = row.get<std::string>(1);
  component.path = row.get<std::string>(2);
  component.version = row.get<std::optional<std::string>>(3);
  component.repositoryId = row.get<std::optional<std::int64_t>>(4);
  const auto clonePath = row.get<std::optional<std::string>>(5);
  const auto clone =
      clonePath && !clonePath->empty()
          ? std::optional<ProjectClone>{ProjectClone{.path = *clonePath}}
          : std::optional<ProjectClone>{};
  return {component.id, effectiveComponentRoot(component, clone)};
}

std::expected<std::vector<ComponentRoot>, std::error_code>
currentComponentRoots(storage::Database &database) {
  auto roots = storage::detail::toItlibGenerator(database.query(
      "SELECT component.id, component.name, component.path, "
      "component.version, component.repository_id, clone.path "
      "FROM component "
      "LEFT JOIN repository ON repository.id=component.repository_id "
      "LEFT JOIN clone ON clone.id=repository.active_clone_id",
      currentComponentRoot));
  return storage::detail::collectGenerator(std::move(roots));
}

std::expected<std::optional<FileIdentity>, std::error_code>
currentSplitIdentity(storage::Database &database, std::string_view identity) {
  return currentComponentRoots(database).transform(
      [&](std::vector<ComponentRoot> roots) {
        return selectIdentity(std::move(roots), identity);
      });
}

std::expected<void, std::error_code>
currentMigratePlaceholderRepository(storage::Database &database,
                                    const ProjectConfiguration &configuration) {
  const std::array configurations{&configuration};
  return database
      .executeBulk(
          "UPDATE repository SET name=?1,remote_url=?2 "
          "WHERE name='facts-tool' AND NOT EXISTS("
          "SELECT 1 FROM repository replacement WHERE replacement.name=?1) "
          "AND EXISTS(SELECT 1 FROM clone WHERE "
          "clone.repository_id=repository.id AND clone.path=?3)",
          configurations,
          [](sqlite3_stmt *statement, const ProjectConfiguration *candidate) {
            return storage::bindParameters(statement, candidate->repositoryName,
                                           candidate->remoteUrl,
                                           candidate->activeClone.path);
          },
          {.atomic = false})
      .transform([](const storage::BulkResult &) {});
}

std::expected<std::int64_t, std::error_code>
currentUpsertRepository(storage::Database &database,
                        const ProjectConfiguration &configuration) {
  auto ids = storage::detail::toItlibGenerator(database.query(
      "INSERT INTO repository(name, remote_url) VALUES(?1, ?2) "
      "ON CONFLICT(name) DO UPDATE SET remote_url=excluded.remote_url "
      "RETURNING id",
      [](const storage::Row &row) { return row.get<std::int64_t>(0); },
      configuration.repositoryName, configuration.remoteUrl));
  return storage::detail::collectOne(std::move(ids));
}

std::expected<std::int64_t, std::error_code>
currentUpsertClone(storage::Database &database, std::int64_t repositoryId,
                   const ProjectClone &clone) {
  auto ids = storage::detail::toItlibGenerator(database.query(
      "INSERT INTO clone(repository_id, path, label) VALUES(?1, ?2, ?3) "
      "ON CONFLICT(path) DO UPDATE SET label=excluded.label RETURNING id",
      [](const storage::Row &row) { return row.get<std::int64_t>(0); },
      repositoryId, clone.path, clone.label));
  return storage::detail::collectOne(std::move(ids));
}

std::expected<void, std::error_code>
currentSetActiveClone(storage::Database &database, std::int64_t repositoryId,
                      std::int64_t cloneId) {
  const std::array rows{repositoryId};
  return database
      .executeBulk("UPDATE repository SET active_clone_id=?1 WHERE id=?2", rows,
                   [cloneId](sqlite3_stmt *statement, std::int64_t id) {
                     return storage::bindParameters(statement, cloneId, id);
                   },
                   {.atomic = false})
      .and_then([](const storage::BulkResult &result)
                    -> std::expected<void, std::error_code> {
        return result.changes == 1
                   ? std::expected<void, std::error_code>{}
                   : std::expected<void, std::error_code>{
                         std::unexpected(std::make_error_code(
                             std::errc::no_such_file_or_directory))};
      });
}

std::expected<FileId, std::error_code>
currentSelectId(storage::Database &database, const FileIdentity &file) {
  auto ids = storage::detail::toItlibGenerator(database.query(
      "SELECT file.id FROM file "
      "JOIN directory ON directory.id=file.directory_id "
      "WHERE directory.component_id=?1 "
      "AND directory.path=?2 AND file.name=?3",
      [](const storage::Row &row) { return row.get<std::int64_t>(0); },
      file.componentId, file.directory, file.name));
  return storage::detail::collectOne(std::move(ids))
      .and_then([](std::int64_t raw) -> std::expected<FileId, std::error_code> {
        if (raw < firstPhysicalFileId ||
            raw > std::numeric_limits<FileId>::max()) {
          return std::unexpected(
              std::make_error_code(std::errc::value_too_large));
        }
        return static_cast<FileId>(raw);
      });
}

std::expected<std::int64_t, std::error_code>
legacyUpsertDirectory(sqlite3 *database, std::int64_t componentId,
                      std::string_view directory) {
  constexpr auto sql =
      "INSERT INTO directory(component_id, path) VALUES(?1, ?2) "
      "ON CONFLICT(component_id, path) DO UPDATE SET path=excluded.path "
      "RETURNING id";
  return legacyPrepare(database, sql).and_then([&](Statement statement) {
    if (sqlite3_bind_int64(statement.get(), 1, componentId) != SQLITE_OK) {
      return std::expected<std::int64_t, std::error_code>{
          std::unexpected(legacySqliteError(database))};
    }
    return legacyBindText(database, statement.get(), 2, directory)
        .and_then([&] { return legacyReadRowId(database, statement.get()); });
  });
}

std::expected<bool, std::error_code> usesLegacyFileSchema(sqlite3 *database) {
  constexpr auto sql = "SELECT EXISTS(SELECT 1 FROM pragma_table_info('file') "
                       "WHERE name='path')";
  return legacyPrepare(database, sql).and_then([&](Statement statement) {
    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
      return std::expected<bool, std::error_code>{
          std::unexpected(legacySqliteError(database))};
    }
    return std::expected<bool, std::error_code>{
        sqlite3_column_int(statement.get(), 0) != 0};
  });
}

std::expected<bool, std::error_code> hasFileColumn(sqlite3 *database,
                                                   std::string_view name) {
  constexpr auto sql =
      "SELECT EXISTS(SELECT 1 FROM pragma_table_info('file') WHERE name=?1)";
  return legacyPrepare(database, sql).and_then([&](Statement statement) {
    return legacyBindText(database, statement.get(), 1, name).and_then([&] {
      if (sqlite3_step(statement.get()) != SQLITE_ROW) {
        return std::expected<bool, std::error_code>{
            std::unexpected(legacySqliteError(database))};
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
                   : legacyExecute(
                         database,
                         "ALTER TABLE file ADD COLUMN working_directory TEXT");
      });
}

std::expected<bool, std::error_code>
usesFlatDirectorySchema(sqlite3 *database) {
  constexpr auto sql =
      "SELECT EXISTS(SELECT 1 FROM pragma_table_info('directory') "
      "WHERE name='path') AND NOT EXISTS(SELECT 1 FROM "
      "pragma_table_info('directory') WHERE name='component_id')";
  return legacyPrepare(database, sql).and_then([&](Statement statement) {
    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
      return std::expected<bool, std::error_code>{
          std::unexpected(legacySqliteError(database))};
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
  return legacyExecute(database, sql);
}

std::expected<void, std::error_code>
insertLegacyFile(sqlite3 *database, FileId id, const FileIdentity &file) {
  return legacyUpsertDirectory(database, file.componentId, file.directory)
      .and_then([&](std::int64_t directoryId) {
        constexpr auto sql =
            "INSERT INTO file(id, directory_id, name) VALUES(?1, ?2, ?3)";
        return legacyPrepare(database, sql).and_then([&](Statement statement) {
          if (sqlite3_bind_int64(statement.get(), 1, id) != SQLITE_OK ||
              sqlite3_bind_int64(statement.get(), 2, directoryId) !=
                  SQLITE_OK) {
            return std::expected<void, std::error_code>{
                std::unexpected(legacySqliteError(database))};
          }
          return legacyBindText(database, statement.get(), 3, file.name)
              .and_then([&] {
                if (sqlite3_step(statement.get()) != SQLITE_DONE) {
                  return std::expected<void, std::error_code>{
                      std::unexpected(legacySqliteError(database))};
                }
                return std::expected<void, std::error_code>{};
              });
        });
      });
}

std::expected<void, std::error_code> migrateLegacyRows(sqlite3 *database) {
  return legacyPrepare(database, "SELECT id, path FROM legacy_file ORDER BY id")
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
              legacySplitIdentity(database,
                                  reinterpret_cast<const char *>(rawPath))
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
              std::unexpected(legacySqliteError(database))};
        }
        return std::expected<void, std::error_code>{};
      });
}

std::expected<void, std::error_code>
migrateLegacyFileSchema(sqlite3 *database) {
  return legacyExecute(database, "ALTER TABLE file RENAME TO legacy_file")
      .and_then([&] { return legacyExecute(database, fileSchemaSql); })
      .and_then([&] { return ensureDefaultComponent(database); })
      .and_then([&] { return migrateLegacyRows(database); })
      .and_then(
          [&] { return legacyExecute(database, "DROP TABLE legacy_file"); });
}

std::expected<void, std::error_code>
migrateFlatDirectoryRows(sqlite3 *database) {
  constexpr auto sql =
      "SELECT legacy_file.id, legacy_directory.path, legacy_file.name "
      "FROM legacy_file JOIN legacy_directory "
      "ON legacy_directory.id=legacy_file.directory_id "
      "ORDER BY legacy_file.id";
  return legacyPrepare(database, sql).and_then([&](Statement statement) {
    auto status = sqlite3_step(statement.get());
    while (status == SQLITE_ROW) {
      const auto rawId = sqlite3_column_int64(statement.get(), 0);
      const auto identity =
          (std::filesystem::path(legacyColumnText(statement.get(), 1)) /
           legacyColumnText(statement.get(), 2))
              .lexically_normal()
              .string();
      if (rawId < firstPhysicalFileId ||
          rawId > std::numeric_limits<FileId>::max()) {
        return std::expected<void, std::error_code>{
            std::unexpected(std::make_error_code(std::errc::invalid_argument))};
      }
      auto inserted =
          legacySplitIdentity(database, identity)
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
          std::unexpected(legacySqliteError(database))};
    }
    return std::expected<void, std::error_code>{};
  });
}

std::expected<void, std::error_code>
migrateFlatDirectorySchema(sqlite3 *database) {
  return legacyExecute(database, "ALTER TABLE file RENAME TO legacy_file")
      .and_then([&] {
        return legacyExecute(
            database, "ALTER TABLE directory RENAME TO legacy_directory");
      })
      .and_then([&] { return legacyExecute(database, fileSchemaSql); })
      .and_then([&] { return ensureDefaultComponent(database); })
      .and_then([&] { return migrateFlatDirectoryRows(database); })
      .and_then(
          [&] { return legacyExecute(database, "DROP TABLE legacy_file"); })
      .and_then([&] {
        return legacyExecute(database, "DROP TABLE legacy_directory");
      });
}

std::expected<void, std::error_code> initializeFileSchema(sqlite3 *database) {
  return storage::Transaction::write(database).and_then(
      [&](storage::Transaction transaction) {
        return usesLegacyFileSchema(database)
            .and_then([&](bool legacy) {
              if (legacy) {
                return migrateLegacyFileSchema(database);
              }
              return usesFlatDirectorySchema(database).and_then([&](bool flat) {
                if (flat) {
                  return migrateFlatDirectorySchema(database);
                }
                return legacyExecute(database, fileSchemaSql)
                    .and_then([&] {
                      return ensureProjectConfigurationSchema(database);
                    })
                    .and_then([&] { return ensureDefaultComponent(database); });
              });
            })
            .and_then([&] { return transaction.commit(); });
      });
}

std::expected<bool, std::error_code> hasTable(sqlite3 *database,
                                              std::string_view name) {
  constexpr auto sql = "SELECT EXISTS(SELECT 1 FROM sqlite_master "
                       "WHERE type='table' AND name=?1)";
  return legacyPrepare(database, sql).and_then([&](Statement statement) {
    return legacyBindText(database, statement.get(), 1, name).and_then([&] {
      if (sqlite3_step(statement.get()) != SQLITE_ROW) {
        return std::expected<bool, std::error_code>{
            std::unexpected(legacySqliteError(database))};
      }
      return std::expected<bool, std::error_code>{
          sqlite3_column_int(statement.get(), 0) != 0};
    });
  });
}

std::expected<void, std::string> requireCompleteFileSchema(sqlite3 *database) {
  constexpr std::array required{"clone", "component",  "directory",
                                "file",  "repository", "semantic_universe"};
  for (const auto *table : required) {
    auto present = hasTable(database, table);
    if (!present) {
      return std::unexpected("cannot read the project configuration: " +
                             present.error().message());
    }
    if (!*present) {
      return std::unexpected(
          "not a project configuration database: the file registry has no '" +
          std::string(table) + "' table");
    }
  }
  return {};
}

// A consumer opens the registry read-only, so an outdated layout cannot be
// migrated in place; say so instead of failing later on an unreadable table.
// An unreadable database, a missing table and an outdated layout each get
// their own message, because each calls for a different response.
std::expected<void, std::string> requireCurrentFileSchema(sqlite3 *database) {
  return requireCompleteFileSchema(database).and_then([&] {
    return usesLegacyFileSchema(database)
        .and_then([&](bool legacy) {
          return legacy ? std::expected<bool, std::error_code>{true}
                        : usesFlatDirectorySchema(database);
        })
        .transform_error([](std::error_code error) {
          return "cannot inspect the project configuration schema: " +
                 error.message();
        })
        .and_then([](bool outdated) {
          return outdated ? std::unexpected(std::string(
                                "project configuration uses an outdated "
                                "file registry; re-run 'facts-tool "
                                "import' to migrate it"))
                          : std::expected<void, std::string>{};
        });
  });
}

std::expected<void, std::string>
requireImportedProjectConfiguration(sqlite3 *database) {
  constexpr auto sql =
      "SELECT EXISTS(SELECT 1 FROM file "
      "WHERE driver IS NOT NULL AND compile_options IS NOT NULL)";
  return legacyPrepare(database, sql)
      .transform_error([](std::error_code error) {
        return "cannot inspect the project configuration: " + error.message();
      })
      .and_then([&](Statement statement) -> std::expected<void, std::string> {
        if (sqlite3_step(statement.get()) != SQLITE_ROW) {
          return std::unexpected("cannot inspect the project configuration: " +
                                 legacySqliteError(database).message());
        }
        return sqlite3_column_int(statement.get(), 0) != 0
                   ? std::expected<void, std::string>{}
                   : std::expected<void, std::string>{std::unexpected(
                         "project configuration is incomplete; run "
                         "'facts-tool import' to rebuild it")};
      });
}

std::expected<storage::Database, std::string>
openReadOnlyFileDatabase(const std::string &path) {
  constexpr int flags = storage::Database::readOnly | SQLITE_OPEN_FULLMUTEX;
  return storage::Database::open(path, flags)
      .transform_error([&](std::error_code error) {
        return "cannot open project configuration read-only: " +
               error.message();
      })
      .and_then([](storage::Database database)
                    -> std::expected<storage::Database, std::string> {
        return database.executeScript("PRAGMA foreign_keys=ON")
            .transform_error([](std::error_code error) {
              return "cannot open project configuration read-only: " +
                     error.message();
            })
            .and_then([&] {
              return requireCurrentFileSchema(database.nativeHandle());
            })
            .transform([&] { return std::move(database); });
      });
}

storage::Database openWritableFileDatabase(const std::string &path) {
  constexpr int flags = storage::Database::readWrite | SQLITE_OPEN_FULLMUTEX;
  auto opened = storage::Database::open(path, flags);
  if (!opened) {
    throw std::runtime_error("cannot open file database: " +
                             opened.error().message());
  }
  auto database = std::move(*opened);
  auto initialized =
      database.executeScript("PRAGMA foreign_keys=ON").and_then([&] {
        return initializeFileSchema(database.nativeHandle());
      });
  if (!initialized) {
    throw std::runtime_error(
        "cannot initialize file database: " +
        std::string{sqlite3_errmsg(database.nativeHandle())});
  }
  return database;
}

} // namespace

FileDatabase::FileDatabase(const std::string &path)
    : database_(openWritableFileDatabase(path)) {}

FileDatabase::FileDatabase(storage::Database database)
    : database_(std::move(database)) {}

std::expected<std::unique_ptr<FileDatabase>, std::string>
FileDatabase::openReadOnly(const std::string &path) {
  return openReadOnlyFileDatabase(path).transform([](storage::Database opened) {
    return std::unique_ptr<FileDatabase>(new FileDatabase(std::move(opened)));
  });
}

std::expected<std::unique_ptr<FileDatabase>, std::string>
FileDatabase::openImportedReadOnly(const std::string &path) {
  return openReadOnlyFileDatabase(path)
      .and_then([](storage::Database opened)
                    -> std::expected<storage::Database, std::string> {
        return requireImportedProjectConfiguration(opened.nativeHandle())
            .transform([&] { return std::move(opened); });
      })
      .transform([](storage::Database opened) {
        return std::unique_ptr<FileDatabase>(
            new FileDatabase(std::move(opened)));
      });
}

FileDatabase::~FileDatabase() = default;

namespace {

std::expected<std::int64_t, std::error_code>
currentFileCount(storage::Database &database) {
  auto counts = storage::detail::toItlibGenerator(database.query(
      "SELECT COUNT(*) FROM file",
      [](const storage::Row &row) { return row.get<std::int64_t>(0); }));
  return storage::detail::collectOne(std::move(counts));
}

} // namespace

std::expected<std::size_t, std::error_code> FileDatabase::fileCount() {
  return currentFileCount(database_).transform(
      [](std::int64_t count) { return static_cast<std::size_t>(count); });
}

std::expected<std::size_t, std::error_code>
FileDatabase::addBulk(std::span<const std::string> identities) {
  auto transaction = database_.write();
  if (!transaction) {
    return std::unexpected(transaction.error());
  }

  // The insert avoids conflicts, so its change count also counts the rows that
  // were already registered. Only the size of the table before and after the
  // statement tells how many files this call actually added.
  auto before = currentFileCount(database_);
  if (!before) {
    return std::unexpected(before.error());
  }

  std::vector<FileIdentity> files;
  for (const auto &identity : identities) {
    auto file = currentSplitIdentity(database_, identity);
    if (!file) {
      return std::unexpected(file.error());
    }
    if (*file) {
      files.push_back(std::move(**file));
    }
  }

  auto directories = database_.executeBulk(
      "INSERT INTO directory(component_id,path) VALUES(?1,?2) "
      "ON CONFLICT(component_id,path) DO NOTHING",
      files,
      [](sqlite3_stmt *statement, const FileIdentity &file) {
        return storage::bindParameters(statement, file.componentId,
                                       file.directory);
      },
      {.atomic = false});
  if (!directories) {
    return std::unexpected(directories.error());
  }

  auto stored = database_.executeBulk(
      "INSERT INTO file(directory_id,name) "
      "SELECT id,?3 FROM directory WHERE component_id=?1 AND path=?2 "
      "ON CONFLICT(directory_id,name) DO UPDATE SET name=excluded.name",
      files,
      [](sqlite3_stmt *statement, const FileIdentity &file) {
        return storage::bindParameters(statement, file.componentId,
                                       file.directory, file.name);
      },
      {.atomic = false});
  if (!stored) {
    return std::unexpected(stored.error());
  }

  auto after = currentFileCount(database_);
  if (!after) {
    return std::unexpected(after.error());
  }
  return transaction->commit().transform(
      [added = static_cast<std::size_t>(*after - *before)] { return added; });
}

std::expected<void, std::string> FileDatabase::replaceProjectConfiguration(
    const ProjectConfiguration &configuration) {
  // Every rejection reason a caller can act on is a field of the
  // configuration, so the storage boundary names it rather than collapsing the
  // lot into "Invalid argument".
  return validateProjectConfiguration(configuration).and_then([&] {
    return storeProjectConfiguration(configuration)
        .transform_error([](std::error_code error) { return error.message(); });
  });
}

std::expected<void, std::error_code> FileDatabase::storeProjectConfiguration(
    const ProjectConfiguration &configuration) {
  auto transaction = database_.write();
  if (!transaction) {
    return std::unexpected(transaction.error());
  }

  return currentMigratePlaceholderRepository(database_, configuration)
      .and_then(
          [&] { return currentUpsertRepository(database_, configuration); })
      .and_then([&](std::int64_t repositoryId) {
        return currentUpsertClone(database_, repositoryId,
                                  configuration.activeClone)
            .and_then([&](std::int64_t cloneId) {
              return currentSetActiveClone(database_, repositoryId, cloneId);
            })
            .and_then([&] {
              return database_.execute(
                  "UPDATE file SET compile_options=NULL, driver=NULL, "
                  "working_directory=NULL");
            })
            .and_then([&] {
              return database_.execute(
                  "UPDATE component SET name='external' "
                  "WHERE repository_id IS NULL AND name='facts-tool' "
                  "AND path='/' AND kind='external'");
            })
            .and_then([&] {
              // validateProjectConfiguration() has already refused an empty
              // or repeated component path, an unnamed file, a file naming an
              // unconfigured component, and a file without compile options.
              std::map<std::string, std::int64_t> componentIds;
              for (const auto &component : configuration.components) {
                componentIds.emplace(component.path, 0);
              }

              auto components = database_.executeBulk(
                  "INSERT INTO component(name,path,kind,version,"
                  "repository_id,semantic_universe_id) "
                  "VALUES(?1,?2,?3,?4,?5,1) "
                  "ON CONFLICT(repository_id,path) DO UPDATE SET "
                  "name=excluded.name,kind=excluded.kind,"
                  "version=excluded.version",
                  configuration.components,
                  [repositoryId](sqlite3_stmt *statement,
                                 const ProjectComponent &component) {
                    return storage::bindParameters(
                        statement, component.name, component.path,
                        component.kind, component.version, repositoryId);
                  },
                  {.atomic = false});
              if (!components) {
                return std::expected<void, std::error_code>{
                    std::unexpected(components.error())};
              }

              auto directories = database_.executeBulk(
                  "INSERT INTO directory(component_id,path) "
                  "SELECT id,?3 FROM component "
                  "WHERE repository_id=?1 AND path=?2 "
                  "ON CONFLICT(component_id,path) DO NOTHING",
                  configuration.files,
                  [repositoryId](sqlite3_stmt *statement,
                                 const ProjectFile &file) {
                    return storage::bindParameters(statement, repositoryId,
                                                   file.componentPath,
                                                   file.directory);
                  },
                  {.atomic = false});
              if (!directories) {
                return std::expected<void, std::error_code>{
                    std::unexpected(directories.error())};
              }

              auto files = database_.executeBulk(
                  "INSERT INTO file(directory_id,name,driver,"
                  "working_directory,compile_options) "
                  "SELECT directory.id,?4,?5,?6,?7 FROM directory "
                  "JOIN component ON component.id=directory.component_id "
                  "WHERE component.repository_id=?1 "
                  "AND component.path=?2 AND directory.path=?3 "
                  "ON CONFLICT(directory_id,name) DO UPDATE SET "
                  "driver=excluded.driver,"
                  "working_directory=excluded.working_directory,"
                  "compile_options=excluded.compile_options",
                  configuration.files,
                  [repositoryId](sqlite3_stmt *statement,
                                 const ProjectFile &file) {
                    const auto workingDirectory =
                        file.workingDirectory.empty()
                            ? std::optional<std::string>{}
                            : std::optional<std::string>{file.workingDirectory};
                    return storage::bindParameters(
                        statement, repositoryId, file.componentPath,
                        file.directory, file.name, file.driver,
                        workingDirectory, file.compileOptions);
                  },
                  {.atomic = false});
              return files.transform([](const storage::BulkResult &) {});
            });
      })
      .and_then([&] { return transaction->commit(); });
}

std::expected<void, std::error_code>
FileDatabase::switchActiveClone(std::string_view repositoryName,
                                std::string_view clonePathOrLabel) {
  auto transaction = database_.write();
  if (!transaction) {
    return std::unexpected(transaction.error());
  }
  constexpr auto sql =
      "UPDATE repository SET active_clone_id=(SELECT id FROM clone "
      "WHERE repository_id=repository.id AND (path=?2 OR label=?2)) "
      "WHERE name=?1 AND EXISTS(SELECT 1 FROM clone "
      "WHERE repository_id=repository.id AND (path=?2 OR label=?2))";
  const std::array rows{std::string{repositoryName}};
  return database_
      .executeBulk(
          sql, rows,
          [clonePathOrLabel](sqlite3_stmt *statement, const std::string &name) {
            return storage::bindParameters(statement, name, clonePathOrLabel);
          },
          {.atomic = false})
      .and_then([](const storage::BulkResult &result)
                    -> std::expected<void, std::error_code> {
        return result.changes == 1
                   ? std::expected<void, std::error_code>{}
                   : std::expected<void, std::error_code>{
                         std::unexpected(std::make_error_code(
                             std::errc::no_such_file_or_directory))};
      })
      .and_then([&] { return transaction->commit(); });
}

std::expected<void, std::error_code>
FileDatabase::addClone(std::string_view repositoryName,
                       const ProjectClone &clone, bool activate) {
  if (repositoryName.empty() || clone.path.empty()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  auto transaction = database_.write();
  if (!transaction) {
    return std::unexpected(transaction.error());
  }
  auto repositoryIds = storage::detail::toItlibGenerator(database_.query(
      "SELECT id FROM repository WHERE name=?1",
      [](const storage::Row &row) { return row.get<std::int64_t>(0); },
      repositoryName));
  return storage::detail::collectOne(std::move(repositoryIds))
      .and_then([&](std::int64_t repositoryId) {
        return currentUpsertClone(database_, repositoryId, clone)
            .and_then([&](std::int64_t cloneId) {
              return activate ? currentSetActiveClone(database_, repositoryId,
                                                      cloneId)
                              : std::expected<void, std::error_code>{};
            });
      })
      .and_then([&] { return transaction->commit(); });
}

std::expected<FileId, std::error_code>
FileDatabase::getId(std::string_view identity) {
  return currentSplitIdentity(database_, identity)
      .and_then([&](const std::optional<FileIdentity> &file) {
        if (!file) {
          return std::expected<FileId, std::error_code>{std::unexpected(
              std::make_error_code(std::errc::no_such_file_or_directory))};
        }
        return currentSelectId(database_, *file);
      });
}

} // namespace facts
