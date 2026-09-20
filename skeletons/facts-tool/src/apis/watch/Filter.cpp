#include "apis/watch/Update.h"
#include "apis/watch/Paths.h"
#include "apis/watch/ignore/Ignore.h"
#include "apis/watch/catalog/Ownership.h"

namespace facts::apis::watch {
namespace {
bool inside(const std::filesystem::path &path, const std::filesystem::path &root) {
  const auto relative = path.lexically_relative(root);
  return !relative.empty() && *relative.begin() != "..";
}
}
std::expected<std::size_t, std::string> acceptedEvents(const Settings &settings,
    const Catalog &catalog, const std::vector<Event> &events) {
  std::size_t count = 0;
  if (events.empty()) return count;
  for (const auto &event : events) if (event.control) ++count;
  for (const auto &clone : catalog.clones) {
    if (!clone.active || !clone.excluded.empty()) continue;
    std::error_code error;
    if (!std::filesystem::is_directory(clone.path, error)) continue;
    auto rules = Ignore::create(clone.path, settings);
    if (!rules) return std::unexpected(rules.error());
    for (const auto &event : events) {
      if (event.control || !inside(event.path, clone.path)) continue;
      const auto *selected = owner(event.path, catalog);
      if (!selected || selected->cloneId != clone.cloneId) continue;
      if (event.path.filename() == ".gitignore") { ++count; continue; }
      if (ignored(event.path, settings) ||
          (!event.directory && !relevant(event.path))) continue;
      auto excluded = rules->excludes(event.path, event.directory);
      if (!excluded) return std::unexpected(excluded.error());
      if (!*excluded) ++count;
    }
  }
  return count;
}
}
