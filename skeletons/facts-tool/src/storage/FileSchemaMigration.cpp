#include "storage/FileSchemaMigration.h"

#include "storage/FileIdentity.h"
#include "storage/FilePersistence.h"
#include "storage/FileSchema.h"
#include "storage/Sqlite.h"

#include <sqlite3.h>

#include <array>
#include <filesystem>
#include <limits>
#include <string_view>

namespace facts {
namespace {

std::expected<void, std::error_code> bindText(sqlite3 *database,
                                              sqlite3_stmt *statement,
                                              int position,
                                              std::string_view value) {
  return storage::bindText(statement, position, value)
             ? std::expected<void, std::error_code>{}
             : std::expected<void, std::error_code>{
                   std::unexpected(storage::sqliteError(database))};
}

std::expected<bool, std::error_code> readBoolean(sqlite3 *database,
                                                 storage::Statement statement) {
  if (sqlite3_step(statement.get()) != SQLITE_ROW) {
    return std::unexpected(storage::sqliteError(database));
  }
  return sqlite3_column_int(statement.get(), 0) != 0;
}

std::expected<bool, std::error_code> usesLegacyFileSchema(sqlite3 *database) {
  constexpr std::string_view sql =
      "SELECT EXISTS(SELECT 1 FROM pragma_table_info('file') "
      "WHERE name='path')";
  return storage::prepare(database, sql)
      .and_then([&](storage::Statement statement) {
        return readBoolean(database, std::move(statement));
      });
}

std::expected<bool, std::error_code> hasFileColumn(sqlite3 *database,
                                                   std::string_view name) {
  constexpr std::string_view sql =
      "SELECT EXISTS(SELECT 1 FROM pragma_table_info('file') WHERE name=?1)";
  return storage::prepare(database, sql)
      .and_then([&](storage::Statement statement) {
        return bindText(database, statement.get(), 1, name).and_then([&] {
          return readBoolean(database, std::move(statement));
        });
      });
}

std::expected<void, std::error_code>
ensureProjectConfigurationSchema(sqlite3 *database) {
  return hasFileColumn(database, "working_directory")
      .and_then([&](bool present) {
        return present
                   ? std::expected<void, std::error_code>{}
                   : storage::execute(
                         database,
                         "ALTER TABLE file ADD COLUMN working_directory TEXT");
      });
}

std::expected<bool, std::error_code>
usesFlatDirectorySchema(sqlite3 *database) {
  constexpr std::string_view sql =
      "SELECT EXISTS(SELECT 1 FROM pragma_table_info('directory') "
      "WHERE name='path') AND NOT EXISTS(SELECT 1 FROM "
      "pragma_table_info('directory') WHERE name='component_id')";
  return storage::prepare(database, sql)
      .and_then([&](storage::Statement statement) {
        return readBoolean(database, std::move(statement));
      });
}

std::expected<void, std::error_code> ensureDefaultComponent(sqlite3 *database) {
  constexpr std::string_view sql =
      "INSERT INTO component(name,path,kind,semantic_universe_id) "
      "SELECT 'facts-tool','/','external',1 "
      "WHERE NOT EXISTS(SELECT 1 FROM component)";
  return storage::execute(database, sql);
}

std::expected<void, std::error_code> migrateLegacyRows(sqlite3 *database) {
  return loadFileIdentityContext(database).and_then(
      [&](FileIdentityContext context) {
        return storage::prepare(database,
                                "SELECT id,path FROM legacy_file ORDER BY id")
            .and_then([&](storage::Statement statement)
                          -> std::expected<void, std::error_code> {
              auto status = sqlite3_step(statement.get());
              while (status == SQLITE_ROW) {
                const auto rawId = sqlite3_column_int64(statement.get(), 0);
                const auto path = storage::columnText(statement.get(), 1);
                if (rawId < firstPhysicalFileId ||
                    rawId > std::numeric_limits<FileId>::max() ||
                    path.empty()) {
                  return std::unexpected(
                      std::make_error_code(std::errc::invalid_argument));
                }
                auto inserted =
                    identifyFile(context.components, context.clone, path)
                        .and_then([&](const FileIdentity &identity) {
                          return insertFileWithId(
                              database, static_cast<FileId>(rawId), identity);
                        });
                if (!inserted) {
                  return inserted;
                }
                status = sqlite3_step(statement.get());
              }
              return status == SQLITE_DONE
                         ? std::expected<void, std::error_code>{}
                         : std::expected<void, std::error_code>{
                               std::unexpected(storage::sqliteError(database))};
            });
      });
}

std::expected<void, std::error_code>
migrateLegacyFileSchema(sqlite3 *database) {
  return storage::execute(database, "ALTER TABLE file RENAME TO legacy_file")
      .and_then([&] { return storage::execute(database, fileSchemaSql); })
      .and_then([&] { return ensureDefaultComponent(database); })
      .and_then([&] { return migrateLegacyRows(database); })
      .and_then(
          [&] { return storage::execute(database, "DROP TABLE legacy_file"); });
}

std::expected<void, std::error_code>
migrateFlatDirectoryRows(sqlite3 *database) {
  constexpr std::string_view sql =
      "SELECT legacy_file.id,legacy_directory.path,legacy_file.name "
      "FROM legacy_file JOIN legacy_directory "
      "ON legacy_directory.id=legacy_file.directory_id "
      "ORDER BY legacy_file.id";
  return loadFileIdentityContext(database).and_then(
      [&](FileIdentityContext context) {
        return storage::prepare(database, sql)
            .and_then([&](storage::Statement statement)
                          -> std::expected<void, std::error_code> {
              auto status = sqlite3_step(statement.get());
              while (status == SQLITE_ROW) {
                const auto rawId = sqlite3_column_int64(statement.get(), 0);
                const auto identity =
                    (std::filesystem::path(
                         storage::columnText(statement.get(), 1)) /
                     storage::columnText(statement.get(), 2))
                        .lexically_normal();
                if (rawId < firstPhysicalFileId ||
                    rawId > std::numeric_limits<FileId>::max()) {
                  return std::unexpected(
                      std::make_error_code(std::errc::invalid_argument));
                }
                auto inserted =
                    identifyFile(context.components, context.clone, identity)
                        .and_then([&](const FileIdentity &file) {
                          return insertFileWithId(
                              database, static_cast<FileId>(rawId), file);
                        });
                if (!inserted) {
                  return inserted;
                }
                status = sqlite3_step(statement.get());
              }
              return status == SQLITE_DONE
                         ? std::expected<void, std::error_code>{}
                         : std::expected<void, std::error_code>{
                               std::unexpected(storage::sqliteError(database))};
            });
      });
}

std::expected<void, std::error_code>
migrateFlatDirectorySchema(sqlite3 *database) {
  return storage::execute(database, "ALTER TABLE file RENAME TO legacy_file")
      .and_then([&] {
        return storage::execute(
            database, "ALTER TABLE directory RENAME TO legacy_directory");
      })
      .and_then([&] { return storage::execute(database, fileSchemaSql); })
      .and_then([&] { return ensureDefaultComponent(database); })
      .and_then([&] { return migrateFlatDirectoryRows(database); })
      .and_then(
          [&] { return storage::execute(database, "DROP TABLE legacy_file"); })
      .and_then([&] {
        return storage::execute(database, "DROP TABLE legacy_directory");
      });
}

} // namespace

std::expected<bool, std::error_code> fileSchemaHasTable(sqlite3 *database,
                                                        std::string_view name) {
  constexpr std::string_view sql = "SELECT EXISTS(SELECT 1 FROM sqlite_master "
                                   "WHERE type='table' AND name=?1)";
  return storage::prepare(database, sql)
      .and_then([&](storage::Statement statement) {
        return bindText(database, statement.get(), 1, name).and_then([&] {
          return readBoolean(database, std::move(statement));
        });
      });
}

namespace {

std::expected<void, std::string> requireCompleteFileSchema(sqlite3 *database) {
  constexpr std::array required{"clone", "component",  "directory",
                                "file",  "repository", "semantic_universe"};
  for (const auto *table : required) {
    auto present = fileSchemaHasTable(database, table);
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

} // namespace

std::expected<void, std::error_code> migrateFileSchema(sqlite3 *database) {
  return storage::Transaction::immediate(database).and_then(
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
                return storage::execute(database, fileSchemaSql)
                    .and_then([&] {
                      return ensureProjectConfigurationSchema(database);
                    })
                    .and_then([&] { return ensureDefaultComponent(database); });
              });
            })
            .and_then([&] { return transaction.commit(); });
      });
}

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
  constexpr std::string_view sql =
      "SELECT EXISTS(SELECT 1 FROM file "
      "WHERE driver IS NOT NULL AND compile_options IS NOT NULL)";
  return storage::prepare(database, sql)
      .transform_error([](std::error_code error) {
        return "cannot inspect the project configuration: " + error.message();
      })
      .and_then([&](storage::Statement statement)
                    -> std::expected<void, std::string> {
        if (sqlite3_step(statement.get()) != SQLITE_ROW) {
          return std::unexpected("cannot inspect the project configuration: " +
                                 storage::sqliteError(database).message());
        }
        return sqlite3_column_int(statement.get(), 0) != 0
                   ? std::expected<void, std::string>{}
                   : std::expected<void, std::string>{std::unexpected(
                         "project configuration is incomplete; run "
                         "'facts-tool import' to rebuild it")};
      });
}

} // namespace facts
