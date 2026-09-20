#include "apis/watch/Update.h"
#include <algorithm>

namespace facts::apis::watch {
std::expected<Update, std::string> update(const Settings &settings,
    std::shared_ptr<const Scan> previous, std::vector<Event> events, bool force,
    const std::atomic_bool &cancelled) {
  auto catalog = readCatalog(settings);
  if (!catalog) return std::unexpected(catalog.error());
  auto count = acceptedEvents(settings, *catalog, events);
  if (!count) return std::unexpected(count.error());
  const bool changed = !previous || previous->catalog != *catalog;
  const bool refresh = force || *count != 0 ||
      (changed && previous && !catalog->clones.empty());
  if (!refresh && !changed && previous->notices.empty())
    return Update{std::move(previous), {}, 0, false};
  auto scanned = discover(settings, std::move(*catalog), cancelled);
  if (!scanned) return std::unexpected(scanned.error());
  auto snapshot = std::make_shared<Scan>(std::move(*scanned));
  const bool recovered = previous && std::ranges::any_of(previous->notices,
      [&](const auto &notice) {
        return std::ranges::find(snapshot->notices, notice) == snapshot->notices.end();
      });
  Update result{snapshot, {}, *count, refresh || recovered};
  if (result.refresh) {
    std::vector<std::filesystem::path> databases(snapshot->databases.begin(),
                                                snapshot->databases.end());
    auto plan = buildPlan(settings, snapshot->catalog, databases);
    if (!plan) return std::unexpected(plan.error());
    result.plan = std::move(*plan);
  }
  return result;
}
}
