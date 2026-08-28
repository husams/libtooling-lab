#include "storage/FileDatabase.h"

#include "storage/FileIdentity.h"
#include "storage/FilePersistence.h"
#include "storage/FileSchemaMigration.h"
#include "storage/ItlibGenerator.h"
#include "storage/StorageQuery.h"

#include <sqlite3.h>

#include <array>
#include <map>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace facts {
namespace {

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
      "INSERT INTO repository(name,remote_url) VALUES(?1,?2) "
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
      "INSERT INTO clone(repository_id,path,label) VALUES(?1,?2,?3) "
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

std::expected<storage::Database, std::string>
openReadOnlyFileDatabase(const std::string &path) {
  constexpr int flags = storage::Database::readOnly | SQLITE_OPEN_FULLMUTEX;
  return storage::Database::open(path, flags)
      .transform_error([](std::error_code error) {
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
        return migrateFileSchema(database.nativeHandle());
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

std::expected<std::size_t, std::error_code> FileDatabase::fileCount() {
  return fileRowCount(database_.nativeHandle())
      .transform(
          [](std::int64_t count) { return static_cast<std::size_t>(count); });
}

std::expected<std::size_t, std::error_code>
FileDatabase::addBulk(std::span<const std::string> identities) {
  auto transaction = database_.write();
  if (!transaction) {
    return std::unexpected(transaction.error());
  }
  return fileRowCount(database_.nativeHandle())
      .and_then([&](std::int64_t before) {
        return loadFileIdentityContext(database_.nativeHandle())
            .and_then([&](const FileIdentityContext &context) {
              return identifyFiles(context, identities);
            })
            .and_then([&](const std::vector<FileIdentity> &files) {
              return persistFiles(database_.nativeHandle(), files);
            })
            .and_then([&] { return fileRowCount(database_.nativeHandle()); })
            .and_then([&](std::int64_t after) {
              return transaction->commit().transform(
                  [=] { return static_cast<std::size_t>(after - before); });
            });
      });
}

std::expected<void, std::string> FileDatabase::replaceProjectConfiguration(
    const ProjectConfiguration &configuration) {
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
                  "UPDATE file SET compile_options=NULL,driver=NULL,"
                  "working_directory=NULL");
            })
            .and_then([&] {
              return database_.execute(
                  "INSERT INTO project_registry(id,complete,fingerprint,"
                  "file_count) VALUES(1,0,'',0) "
                  "ON CONFLICT(id) DO UPDATE SET complete=0,fingerprint='',"
                  "file_count=0");
            })
            .and_then([&] {
              return database_.execute(
                  "UPDATE component SET name='external' "
                  "WHERE repository_id IS NULL AND name='facts-tool' "
                  "AND path='/' AND kind='external'");
            })
            .and_then([&] {
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

std::expected<RegistryStatus, std::error_code> FileDatabase::registryStatus() {
  auto present =
      fileSchemaHasTable(database_.nativeHandle(), "project_registry");
  if (!present) {
    return std::unexpected(present.error());
  }
  if (!*present) {
    return RegistryStatus{};
  }
  auto rows = storage::detail::toItlibGenerator(database_.query(
      "SELECT complete,fingerprint,file_count FROM project_registry WHERE id=1",
      [](const storage::Row &row) {
        return RegistryStatus{
            .complete = row.get<std::int64_t>(0) != 0,
            .fingerprint = row.get<std::string>(1),
            .fileCount = static_cast<std::size_t>(row.get<std::int64_t>(2))};
      }));
  return storage::detail::collectOptional(std::move(rows))
      .transform([](std::optional<RegistryStatus> status) {
        return status.value_or(RegistryStatus{});
      });
}

std::expected<void, std::error_code>
FileDatabase::markRegistryComplete(std::string_view fingerprint) {
  auto transaction = database_.write();
  if (!transaction) {
    return std::unexpected(transaction.error());
  }
  return fileRowCount(database_.nativeHandle())
      .and_then([&](std::int64_t files) {
        const std::array rows{std::string{fingerprint}};
        return database_
            .executeBulk(
                "INSERT INTO project_registry(id,complete,fingerprint,"
                "file_count) VALUES(1,1,?1,?2) "
                "ON CONFLICT(id) DO UPDATE SET complete=1,"
                "fingerprint=excluded.fingerprint,"
                "file_count=excluded.file_count",
                rows,
                [files](sqlite3_stmt *statement, const std::string &value) {
                  return storage::bindParameters(statement, value, files);
                },
                {.atomic = false})
            .transform([](const storage::BulkResult &) {});
      })
      .and_then([&] { return transaction->commit(); });
}

std::expected<FileId, std::error_code>
FileDatabase::getId(std::string_view identity) {
  return loadFileIdentityContext(database_.nativeHandle())
      .and_then([&](const FileIdentityContext &context) {
        return identifyFile(context.components, context.clone,
                            std::filesystem::path(identity));
      })
      .and_then([&](const FileIdentity &file) {
        return selectFileId(database_.nativeHandle(), file);
      });
}

} // namespace facts
