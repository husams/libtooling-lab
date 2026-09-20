#include "apis/watch/State.h"
#ifdef __linux__
#include <sys/inotify.h>
#include <cerrno>
#include <cstring>
#include <algorithm>

namespace facts::apis {
std::expected<void, std::string>
Watcher::Impl::applyScan(const watch::Scan &result) {
  if (logger) for (const auto &warning : result.warnings) {
    if (snapshot && std::ranges::find(snapshot->warnings, warning) != snapshot->warnings.end())
      continue;
    logger->write(logging::Level::warning, "watch.scan.warning",
        {{"code", warning.code}, {"path", warning.path.string()},
         {"target", warning.target.string()}, {"action", "skipped"},
         {"message", warning.message}});
  }
  constexpr unsigned mask = IN_CLOSE_WRITE | IN_MOVED_TO | IN_MOVED_FROM |
      IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_ONLYDIR;
  std::map<int, std::filesystem::path> nextWatches;
  for (const auto &path : result.directories) {
    const int handle = inotify_add_watch(descriptor.native_handle(),
                                          path.c_str(), mask);
    if (handle < 0 && (errno == ENOENT || errno == ENOTDIR || errno == ELOOP ||
                       errno == EACCES || errno == EPERM)) {
      needsScan = true;
      continue;
    }
    if (handle < 0)
      return std::unexpected("cannot watch " + path.string() + ": " +
                             std::strerror(errno));
    // inotify returns the same descriptor for aliases of one directory.
    // Retain one stable lexical path rather than replacing it with a cycle.
    nextWatches.try_emplace(handle, path);
  }
  for (const auto &[handle, path] : watches)
    if (!nextWatches.contains(handle)) inotify_rm_watch(descriptor.native_handle(), handle);
  watches = std::move(nextWatches);
  return {};
}
}
#endif
