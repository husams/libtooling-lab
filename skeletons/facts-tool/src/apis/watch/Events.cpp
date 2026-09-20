#include "apis/watch/State.h"
#ifdef __linux__
#include <boost/asio/buffer.hpp>
#include <sys/inotify.h>
#include <algorithm>
#include <cstring>

namespace facts::apis {
void Watcher::Impl::read() {
  descriptor.async_read_some(boost::asio::buffer(buffer),
      [weak = weak_from_this()](boost::system::error_code result, std::size_t bytes) {
        auto self = weak.lock();
        if (!self || !self->running) return;
        if (result) {
          self->error = result.message();
          if (self->logger) self->logger->write(logging::Level::warning, "watch.failed",
                                               {{"stage", "read"}});
          self->stop();
          return;
        }
        self->consume(bytes);
        if (self->running) self->read();
      });
}

void Watcher::Impl::consume(std::size_t bytes) {
  for (std::size_t offset = 0; offset + sizeof(inotify_event) <= bytes;) {
    inotify_event record{};
    std::memcpy(&record, buffer.data() + offset, sizeof(record));
    if (record.len > bytes - offset - sizeof(record)) break;
    const char *name = buffer.data() + offset + sizeof(record);
    const auto length = record.len ? strnlen(name, record.len) : 0;
    event(record.wd, record.mask, std::string(name, length));
    offset += sizeof(record) + record.len;
  }
}

void Watcher::Impl::event(int handle, unsigned mask, const std::string &name) {
  if (mask & IN_Q_OVERFLOW) {
    ++overflows;
    if (logger) logger->write(logging::Level::warning, "watch.overflow",
                              {{"overflows", overflows}});
    needsScan = true;
    changed();
    return;
  }
  const auto found = watches.find(handle);
  if (found == watches.end()) return;
  const auto path = name.empty() ? found->second : found->second / name;
  if (!settings.logging.file.empty() &&
      watch::absolute(path, settings) == settings.logging.file) return;
  if (mask & IN_IGNORED) {
    watches.erase(found);
    needsScan = true;
    changed();
    return;
  }
  const bool control = snapshot && (snapshot->controlFiles.contains(path) ||
      std::ranges::any_of(snapshot->controlFiles, [&](const auto &file) {
        return file.parent_path() == path;
      }));
  if (!control && watch::ignored(path, settings)) return;
  if (mask & (IN_DELETE_SELF | IN_MOVE_SELF)) needsScan = true;
  const bool directory = mask & (IN_ISDIR | IN_DELETE_SELF | IN_MOVE_SELF);
  const bool saved = mask & (IN_CLOSE_WRITE | IN_MOVED_TO | IN_MOVED_FROM |
                             IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF);
  if (!directory && (!saved || (!control && !watch::relevant(path) &&
                                path.filename() != ".gitignore"))) return;
  if (pendingEvents.size() >= 4096) {
    pendingEvents.clear();
    needsScan = true;
  } else pendingEvents.push_back({path, directory, control});
  if (logger) logger->write(logging::Level::trace, "watch.event",
      {{"directory", directory}, {"control", control}, {"pending", pendingEvents.size()}});
  changed();
}
}
#endif
