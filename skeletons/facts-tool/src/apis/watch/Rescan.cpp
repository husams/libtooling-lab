#include "apis/watch/State.h"
#ifdef __linux__
#include <boost/asio/post.hpp>
#include <chrono>

namespace facts::apis {
void Watcher::Impl::scan() {
  scanning = true;
  needsScan = false;
  boost::asio::post(scanner, [weak = weak_from_this()] {
    auto self = weak.lock();
    if (!self) return;
    auto result = watch::discover(self->settings, self->cancelled);
    boost::asio::post(self->io, [weak, result = std::move(result)]() mutable {
      if (auto self = weak.lock(); self && self->running)
        self->scanned(std::move(result));
    });
  });
}

void Watcher::Impl::scanned(std::expected<watch::Scan, std::string> result) {
  scanning = false;
  auto applied = result.and_then([&](const auto &plan) { return applyScan(plan); });
  ready = applied.has_value();
  if (!applied) {
    error = applied.error();
    finished(false);
    retry();
    return;
  }
  recovery.cancel();
  prepareImports();
  importNext();
}

void Watcher::Impl::retry() {
  recovery.expires_after(std::chrono::seconds(1));
  recovery.async_wait([weak = weak_from_this()](boost::system::error_code result) {
    if (auto self = weak.lock(); self && self->running && !result) {
      self->needsScan = true;
      self->changed();
    }
  });
}
}
#endif
