#include "apis/watch/State.h"
#include "apis/watch/Snapshot.h"
#ifdef __linux__
#include <sys/inotify.h>
#include <cerrno>
#include <cstring>
#endif

namespace facts::apis {
Watcher::Impl::Impl(boost::asio::io_context &io, Queue &jobs,
                    const Settings &configuration, logging::Logger *log)
    :
#ifdef __linux__
      descriptor(io),
#endif
      queue(jobs), io(io), settings(configuration), logger(log), debounce(io), recovery(io) {}

std::expected<void, std::string> Watcher::Impl::start() {
  if (running || !settings.watchEnabled) return {};
#ifdef __linux__
  const int handle = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (handle < 0) return std::unexpected(std::strerror(errno));
  descriptor.assign(handle);
  cancelled = false;
  auto initial = watch::readCatalog(settings).and_then([&](watch::Catalog catalog) {
    return watch::discover(settings, std::move(catalog), cancelled);
  });
  if (!initial) { descriptor.close(); return std::unexpected(initial.error()); }
  auto result = applyScan(*initial);
  if (!result) { descriptor.close(); return result; }
  snapshot = std::make_shared<watch::Scan>(std::move(*initial));
  running = true;
  ready = snapshot->notices.empty();
  if (logger) logger->write(logging::Level::info, "watch.started",
      {{"roots", snapshot->roots.size()}, {"directories", watches.size()}, {"ready", ready}});
  read();
  poll();
  // Build/import failures belong to background status; a malformed compilation
  // database must not prevent the server from starting and being repaired.
  resumed = watch::restored(settings, *snapshot);
  needsScan = !resumed && !snapshot->catalog.clones.empty();
  if (needsScan) scan();
  return {};
#else
  return std::unexpected("filesystem monitoring requires Linux inotify");
#endif
}

void Watcher::Impl::stop() {
  if (running && logger) logger->write(logging::Level::info, "watch.stopped",
      {{"cycles", cycles}, {"failures", failures}});
  running = false;
  cancelled = true;
  ready = false;
  checkpointPending = false;
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
                 const Settings &settings, logging::Logger *logger)
    : impl_(std::make_shared<Impl>(io, queue, settings, logger)) {}
Watcher::~Watcher() { stop(); impl_->scanner.join(); }
std::expected<void, std::string> Watcher::start() { return impl_->start(); }
void Watcher::stop() { impl_->stop(); }
std::expected<void, std::string> Watcher::reconfigure(const Settings &settings) {
  const auto previous = impl_->settings;
  auto &io = impl_->io;
  auto &queue = impl_->queue;
  auto *logger = impl_->logger;
  impl_->stop();
  // Cancellation is observed inside traversal. Join before replacing state so
  // an old scan cannot publish into the newly configured watcher.
  impl_->scanner.join();
  impl_ = std::make_shared<Impl>(io, queue, settings, logger);
  auto started = impl_->start();
  if (started) return {};
  const auto error = started.error();
  impl_ = std::make_shared<Impl>(io, queue, previous, logger);
  auto restored = impl_->start();
  return std::unexpected(error + (restored ? "" : "; previous watcher could not restart: " + restored.error()));
}
void Watcher::reconcile() {
  if (!impl_->running) return;
  impl_->needsScan = true;
  impl_->changed();
}
Json Watcher::status() const { return impl_->status(); }
}
