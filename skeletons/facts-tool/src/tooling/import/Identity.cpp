#include "tooling/import/Identity.h"
#include "tooling/StoredCompilationReader.h"
#include "storage/Sqlite.h"

namespace facts {
namespace {
std::expected<ProjectConfiguration, std::string>
readClone(sqlite3 *database, std::int64_t cloneId) {
  return storage::prepare(database,
      "SELECT r.id,r.name,r.remote_url,c.path,c.label FROM repository r "
      "JOIN clone c ON c.id=r.active_clone_id "
      "WHERE c.id=?1 AND c.repository_id=r.id")
      .transform_error([](auto error) { return error.message(); })
      .and_then([&](storage::Statement query)
                    -> std::expected<ProjectConfiguration, std::string> {
        sqlite3_bind_int64(query.get(), 1, cloneId);
        const int status = sqlite3_step(query.get());
        if (status != SQLITE_ROW)
          return std::unexpected(status == SQLITE_DONE
              ? "import requires an existing active clone"
              : std::string(sqlite3_errmsg(database)));
        ProjectConfiguration result;
        result.repositoryName = storage::columnText(query.get(), 1);
        result.remoteUrl = storage::columnText(query.get(), 2);
        result.activeClone = {cloneId, sqlite3_column_int64(query.get(), 0),
            storage::columnText(query.get(), 3), storage::columnText(query.get(), 4)};
        return result;
      });
}

std::expected<ProjectConfiguration, std::string>
readComponents(sqlite3 *database, ProjectConfiguration configuration) {
  return storage::prepare(database,
      "SELECT id,name,path,kind,version FROM component "
      "WHERE repository_id=?1 ORDER BY id")
      .transform_error([](auto error) { return error.message(); })
      .and_then([&](storage::Statement query)
                    -> std::expected<ProjectConfiguration, std::string> {
        sqlite3_bind_int64(query.get(), 1, configuration.activeClone.repositoryId);
        int status;
        while ((status = sqlite3_step(query.get())) == SQLITE_ROW) {
          ProjectComponent component;
          component.id = sqlite3_column_int64(query.get(), 0);
          component.name = storage::columnText(query.get(), 1);
          component.path = storage::columnText(query.get(), 2);
          component.kind = storage::columnText(query.get(), 3);
          if (sqlite3_column_type(query.get(), 4) != SQLITE_NULL)
            component.version = storage::columnText(query.get(), 4);
          component.repositoryId = configuration.activeClone.repositoryId;
          configuration.components.push_back(std::move(component));
        }
        if (status != SQLITE_DONE)
          return std::unexpected(sqlite3_errmsg(database));
        return std::move(configuration);
      });
}
}

std::expected<ProjectConfiguration, std::string>
readImportIdentity(const std::filesystem::path &path, std::int64_t cloneId) {
  return openStoredDatabase(path).and_then([&](StoredDatabase database) {
    return readClone(database.get(), cloneId).and_then([&](auto configuration) {
      return readComponents(database.get(), std::move(configuration));
    });
  });
}
}
