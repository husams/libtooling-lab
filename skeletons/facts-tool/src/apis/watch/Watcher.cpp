#include "apis/watch/State.h"
#ifdef __linux__
#include <sys/inotify.h>
#include <cerrno>
#include <cstring>
#endif

namespace facts::apis {
Watcher::Impl::Impl(boost::asio::io_context &io, Queue &jobs,
                    const Settings &configuration)
    :
#ifdef __linux__
      descriptor(io),
#endif
      queue(jobs), io(io), settings(configuration), debounce(io), recovery(io) {}

std::expected<void, std::string> Watcher::Impl::start() {
  if (running || !settings.watchEnabled) return {};
#ifdef __linux__
  const int handle = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (handle < 0) return std::unexpected(std::strerror(errno));
  descriptor.assign(handle);
  cancelled = false;
  auto initial = watch::update(settings, {}, {}, false, cancelled);
  if (!initial) { descriptor.close(); return std::unexpected(initial.error()); }
  auto result = applyScan(*initial->snapshot);
  if (!result) { descriptor.close(); return result; }
  snapshot = std::move(initial->snapshot);
  running = true;
  ready = snapshot->notices.empty();
  read();
  poll();
  return {};
#else
  return std::unexpected("filesystem monitoring requires Linux inotify");
#endif
}

void Watcher::Impl::stop() {
  running = false;
  cancelled = true;
  ready = false;
  active = false;
  dirty = false;
  pendingEvents.clear();
  commands.clear();
  debounce.cancel();
  recovery.cancel();
#ifdef __linux__
  boost::system::error_code ignored;
  descriptor.close(ignored);
  watches.clear();
#endif
}

Watcher::Watcher(boost::asio::io_context &io, Queue &queue,
                 const Settings &settings)
    : impl_(std::make_shared<Impl>(io, queue, settings)) {}
Watcher::~Watcher() { stop(); impl_->scanner.join(); }
std::expected<void, std::string> Watcher::start() { return impl_->start(); }
void Watcher::stop() { impl_->stop(); }
Json Watcher::status() const { return impl_->status(); }
}
