#include "apis/watch/State.h"
#ifdef __linux__
#include <sys/inotify.h>
#include <cerrno>
#include <cstring>

namespace facts::apis {
std::expected<void, std::string>
Watcher::Impl::applyScan(const watch::Scan &result) {
  constexpr unsigned mask = IN_CLOSE_WRITE | IN_MOVED_TO | IN_MOVED_FROM |
      IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_ONLYDIR;
  for (const auto &path : result.directories) {
    const int handle = inotify_add_watch(descriptor.native_handle(),
                                          path.c_str(), mask);
    if (handle < 0)
      return std::unexpected("cannot watch " + path.string() + ": " +
                             std::strerror(errno));
    watches.insert_or_assign(handle, path);
  }
  for (auto it = watches.begin(); it != watches.end();) {
    if (result.directories.contains(it->second)) { ++it; continue; }
    inotify_rm_watch(descriptor.native_handle(), it->first);
    it = watches.erase(it);
  }
  return {};
}
}
#endif
