#include "apis/watch/State.h"
#ifdef __linux__
#include <boost/asio/buffer.hpp>
#include <sys/inotify.h>
#include <cstring>

namespace facts::apis {
void Watcher::Impl::read() {
  descriptor.async_read_some(boost::asio::buffer(buffer),
      [weak = weak_from_this()](boost::system::error_code result,
                                std::size_t bytes) {
        auto self = weak.lock();
        if (!self || !self->running) return;
        if (result) { self->error = result.message(); self->stop(); return; }
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
    needsScan = true;
    changed();
    return;
  }
  const auto found = watches.find(handle);
  if (found == watches.end()) return;
  const auto path = name.empty() ? found->second : found->second / name;
  if (mask & IN_IGNORED) { watches.erase(found); return; }
  if (watch::ignored(path, settings)) return;
  if (mask & (IN_DELETE_SELF | IN_MOVE_SELF)) {
    needsScan = true;
    changed();
    return;
  }
  const bool saved = mask & (IN_CLOSE_WRITE | IN_MOVED_TO | IN_MOVED_FROM |
                             IN_DELETE);
  if ((mask & IN_ISDIR) || (saved && watch::relevant(path))) {
    needsScan = needsScan || (mask & IN_ISDIR) ||
                path.filename() == "compile_commands.json";
    ++events;
    changed();
  }
}
}
#endif
