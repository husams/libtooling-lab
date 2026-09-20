#include "apis/index/Internal.h"
#include <set>

namespace facts::apis::index {
Result<RefreshResult> refresh(const std::filesystem::path &project,
    const std::vector<std::filesystem::path> &configuredSources,
    const std::filesystem::path &factsBase) {
  return catalog::open(project.string(), true).and_then([&](Database database) {
    return initialize(database)
        .and_then([&] { return prepareStage(database); })
        .and_then([&] {
          return prepareOwners(database, factsBase.empty() ? project.parent_path() : factsBase);
        })
        .and_then([&] {
          return configuredSources.empty() ? sources(database, project)
              : Result<std::vector<std::filesystem::path>>{configuredSources};
        })
        .and_then([&](const auto &paths) -> Result<RefreshResult> {
          std::set<std::filesystem::path> all;
          for (const auto &path : paths) {
            std::error_code error;
            const auto full = path.is_absolute() ? path : project.parent_path() / path;
            const auto canonical = std::filesystem::weakly_canonical(full, error);
            if (error) return std::unexpected("cannot resolve facts database: " + error.message());
            all.insert(canonical);
          }
          std::size_t missing = 0;
          for (const auto &path : all) {
            auto result = copySource(database, path);
            if (!result) return std::unexpected(result.error());
            if (!*result) ++missing;
          }
          return publish(database, all.size() - missing, missing);
        });
  });
}
}
