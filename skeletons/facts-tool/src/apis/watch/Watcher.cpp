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
  if (running || settings.directories.empty()) return {};
#ifdef __linux__
  const int handle = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (handle < 0) return std::unexpected(std::strerror(errno));
  descriptor.assign(handle);
  cancelled = false;
  auto result = watch::discover(settings, cancelled)
      .and_then([&](const auto &plan) { return applyScan(plan); });
  if (!result) { descriptor.close(); return result; }
  running = true;
  ready = true;
  read();
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
