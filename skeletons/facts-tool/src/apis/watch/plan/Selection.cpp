#include "apis/watch/plan/Details.h"
#include "apis/watch/Paths.h"
#include "apis/watch/catalog/Ownership.h"

namespace facts::apis::watch::plan {
std::expected<std::vector<std::string>, std::string> selectSources(
    const std::vector<std::string> &sources, const Catalog &catalog,
    const Clone &clone, const Ignore &ignore, const Settings &settings) {
  std::vector<std::string> result;
  for (const auto &value : sources) {
    const auto path = std::filesystem::path(value).lexically_normal();
    const auto *selected = owner(path, catalog);
    if (!selected || selected->cloneId != clone.cloneId || watch::ignored(path, settings)) continue;
    auto excluded = ignore.excludes(path, false);
    if (!excluded) return std::unexpected(excluded.error());
    if (*excluded) continue;
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
      if (error && error != std::errc::no_such_file_or_directory)
        return std::unexpected("cannot read watched source: " + error.message());
      continue;
    }
    const auto canonical = std::filesystem::weakly_canonical(path, error);
    if (error) return std::unexpected("cannot resolve watched source: " + error.message());
    const auto *target = owner(canonical, catalog);
    if (!target || target->cloneId != clone.cloneId) continue;
    auto targetExcluded = ignore.excludes(canonical, false);
    if (!targetExcluded) return std::unexpected(targetExcluded.error());
    if (!*targetExcluded) result.push_back(path.string());
  }
  return result;
}
}
