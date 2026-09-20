#include "apis/watch/catalog/Catalog.h"
#include "apis/watch/catalog/Exclusions.h"
#include "config/Configuration.h"
#include "storage/catalog/Database.h"
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA256.h>

namespace facts::apis::watch {
namespace {
config::Request request(const Settings &settings) {
  config::Request result;
  for (std::size_t index = 0; index + 1 < settings.defaults.size(); index += 2) {
    const auto &option = settings.defaults[index];
    if (option == "--config") result.selector = settings.defaults[index + 1];
    if (option == "--conf") result.direct = settings.defaults[index + 1];
  }
  return result;
}
std::expected<std::vector<Clone>, std::string>
readClones(catalog::Database &database, const Settings &settings) {
  return catalog::query(database,
      "SELECT r.id,c.id,r.name,coalesce(c.label,''),c.path,"
      "coalesce(r.active_clone_id=c.id,0) FROM repository r "
      "JOIN clone c ON c.repository_id=r.id ORDER BY r.id,c.id",
      [&](const storage::Row &row) {
        Clone clone{row.integer(0), row.integer(1), row.string(2),
                    row.string(3), row.string(4), row.integer(5) != 0, {}};
        clone.excluded = exclusion(clone, settings);
        return clone;
      });
}
// Deliberately omit extraction/index timestamps and facts paths: publishing
// facts must not look like a new compiler configuration on the next scan.
std::expected<std::string, std::string> compilationState(catalog::Database &database) {
  llvm::SHA256 hash;
  const auto append = [&](std::string_view value) {
    hash.update(llvm::StringRef(std::to_string(value.size()) + ":"));
    hash.update(llvm::StringRef(value.data(), value.size()));
  };
  const auto rows = [&](const std::string &sql) {
    append(sql);
    for (const auto &row : database.rows(sql))
      for (int column = 0; column < row.columns(); ++column) {
        append(row.isNull(column) ? "null" : "value");
        append(row.text(column));
      }
  };
  try {
    rows("SELECT id,name,path,kind,version,repository_id FROM component ORDER BY id");
    rows("SELECT f.id,c.id,d.path,f.name,f.driver,f.working_directory,"
         "f.compile_options,f.args_overridden FROM file f "
         "JOIN directory d ON d.id=f.directory_id JOIN component c ON c.id=d.component_id "
         "WHERE coalesce(f.driver,'')!='' ORDER BY f.id");
    const auto labels = catalog::query(database,
        "SELECT EXISTS(SELECT 1 FROM sqlite_master WHERE type='table' AND name='label')",
        [](const storage::Row &row) { return row.integer(0) != 0; });
    if (!labels) return std::unexpected(labels.error());
    if (labels->front()) rows("SELECT name,path FROM label ORDER BY name");
    std::string digest;
    constexpr char hex[] = "0123456789abcdef";
    for (auto byte : hash.final()) {
      digest += hex[byte >> 4];
      digest += hex[byte & 15];
    }
    return digest;
  } catch (const storage::QueryError &error) {
    return std::unexpected(error.what());
  }
}
std::expected<Catalog, std::string>
readState(const std::filesystem::path &path, const Settings &settings) {
  std::error_code error;
  const bool exists = std::filesystem::exists(path, error);
  if (error) return std::unexpected("cannot inspect project database: " + error.message());
  if (!exists) return Catalog{path, {}, {}};
  return catalog::open(path.string(), false)
      .and_then([&](catalog::Database database) {
        return database.read().transform_error([](auto error) { return error.message(); })
            .and_then([&](storage::Transaction transaction) {
              return readClones(database, settings).and_then([&](auto clones) {
                return compilationState(database).transform([&](auto state) {
                  return Catalog{path, std::move(clones), std::move(state)};
                });
              });
            });
      });
}
}
std::expected<Catalog, std::string> readCatalog(const Settings &settings) {
  return config::resolve(request(settings))
      .and_then([&](const config::Resolved &resolved) {
        return readState(resolved.database, settings);
      });
}
}
