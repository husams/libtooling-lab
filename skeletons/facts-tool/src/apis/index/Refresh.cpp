#include "apis/index/Internal.h"
#include <set>

namespace facts::apis::index {
namespace {
Result<std::set<std::filesystem::path>> canonicalSources(const std::vector<std::filesystem::path> &paths,
                                                       const std::filesystem::path &project) {
  std::set<std::filesystem::path> all;
  for (const auto &path : paths) {
    std::error_code error;
    const auto full = path.is_absolute() ? path : project.parent_path() / path;
    const auto canonical = std::filesystem::weakly_canonical(full, error);
    if (error) return std::unexpected("cannot resolve facts database " + full.string() + ": " + error.message());
    all.insert(canonical);
  }
  return all;
}
Result<bool> blockedSource(Database &database, const std::string &path) {
  return catalog::query(database,
      "SELECT EXISTS(SELECT 1 FROM temp.api_symbol_owner WHERE facts_db=?1) AND NOT EXISTS "
      "(SELECT 1 FROM temp.api_symbol_owner o WHERE facts_db=?1 AND NOT EXISTS "
      "(SELECT 1 FROM api_index_invalidated_file i WHERE i.file_id=o.file_id))",
      [](const storage::Row &row) { return row.integer(0) != 0; }, path)
      .transform([](const auto &rows) { return rows.at(0); });
}
Result<void> refreshSource(Database &database, const std::filesystem::path &path, RefreshResult &counts) {
  return sourceFingerprint(path).and_then([&](const std::string &fingerprint) {
    return blockedSource(database, path.string()).and_then([&](bool blocked) {
      const auto stamp = blocked ? "blocked" : fingerprint;
      const bool missing = fingerprint.starts_with("missing;");
      counts.missingSources += missing;
      counts.sources += !missing;
      return catalog::query(database, "SELECT fingerprint FROM api_index_source WHERE path=?",
          [](const storage::Row &row) { return row.string(0); }, path.string())
          .and_then([&](const auto &saved) -> Result<void> {
            if (!saved.empty() && saved.at(0) == stamp) { ++counts.skippedSources; return {}; }
            return catalog::execute(database, "DELETE FROM api_index_symbol WHERE source=?", path.string())
                .and_then([&] { return copySource(database, path); })
                .and_then([&](bool) { return sourceFingerprint(path); })
                .and_then([&](const std::string &after) -> Result<void> {
                  if (after != fingerprint)
                    return std::unexpected("facts database changed during indexing: " + path.string() + "; retry the index job");
                  ++counts.processedSources;
                  return catalog::execute(database,
                      "INSERT INTO api_index_source(path,fingerprint) VALUES(?,?) "
                      "ON CONFLICT(path) DO UPDATE SET fingerprint=excluded.fingerprint", path.string(), stamp);
                });
          });
    });
  });
}
Result<RefreshResult> refreshSources(Database &database, const std::set<std::filesystem::path> &paths) {
  RefreshResult counts;
  auto old = catalog::query(database, "SELECT path FROM api_index_source",
      [](const storage::Row &row) { return row.string(0); });
  if (!old) return std::unexpected(old.error());
  for (const auto &path : *old) {
    if (paths.contains(path)) continue;
    auto removed = catalog::execute(database, "DELETE FROM api_index_symbol WHERE source=?", path)
        .and_then([&] { return catalog::execute(database, "DELETE FROM api_index_source WHERE path=?", path); });
    if (!removed) return std::unexpected(removed.error());
    ++counts.removedSources;
  }
  for (const auto &path : paths) {
    auto updated = refreshSource(database, path, counts);
    if (!updated) return std::unexpected(updated.error());
  }
  return stageCachedSources(database)
      .and_then([&] { return publish(database, counts.sources, counts.missingSources); })
      .transform([&](RefreshResult result) {
        result.processedSources = counts.processedSources;
        result.skippedSources = counts.skippedSources;
        result.removedSources = counts.removedSources;
        return result;
      });
}
}
Result<RefreshResult> refresh(const std::filesystem::path &project,
    const std::vector<std::filesystem::path> &configuredSources,
    const std::filesystem::path &factsBase) {
  return catalog::open(project.string(), true).and_then([&](Database database) {
    return initialize(database).and_then([&] {
      return database.write().transform_error([&](auto) { return catalog::databaseError(database); });
    })
        .and_then([&](storage::Transaction transaction) {
          return prepareStage(database)
              .and_then([&] { return prepareOwners(database, factsBase.empty() ? project.parent_path() : factsBase); })
              .and_then([&] { return configuredSources.empty() ? sources(database, project)
                  : Result<std::vector<std::filesystem::path>>{configuredSources}; })
              .and_then([&](const auto &paths) { return canonicalSources(paths, project); })
              .and_then([&](const auto &paths) { return refreshSources(database, paths); })
              .and_then([&](RefreshResult result) {
                return transaction.commit().transform_error([&](auto) { return catalog::databaseError(database); })
                    .transform([&] { return result; });
              });
        });
  });
}
}
