#include "storage/ProjectSchema.h"

#include "storage/ProjectSchemaInternal.h"

namespace facts {

project_schema::Result requireSupportedProjectSchema(sqlite3 *database) {
  return project_schema::hasVersionColumn(database).and_then(
      [&](bool present) -> project_schema::Result {
        return present ? project_schema::version(database).and_then(
                             project_schema::rejectNewer)
                       : project_schema::Result{};
      });
}

project_schema::Result requireCurrentProjectSchema(sqlite3 *database) {
  return project_schema::hasVersionColumn(database).and_then(
      [&](bool present) -> project_schema::Result {
        if (!present)
          return std::unexpected(
              "project configuration uses an outdated project "
              "schema; re-run 'facts-tool import' to migrate it");
        return project_schema::version(database).and_then(
            [](int value) -> project_schema::Result {
              if (value == currentProjectSchemaVersion)
                return {};
              return std::unexpected(
                  value > currentProjectSchemaVersion
                      ? "unsupported-project-schema: " + std::to_string(value)
                      : "project configuration uses an outdated project "
                        "schema; re-run 'facts-tool import' to migrate it");
            });
      });
}

} // namespace facts
