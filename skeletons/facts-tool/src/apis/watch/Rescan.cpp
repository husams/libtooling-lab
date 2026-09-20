#include "apis/watch/State.h"
#ifdef __linux__
#include <boost/asio/post.hpp>
#include <chrono>
#include <utility>

namespace facts::apis {
void Watcher::Impl::scan() {
  if (active || scanning || !running) return;
  scanning = true;
  dirty = false;
  auto changes = std::exchange(pendingEvents, {});
  const bool force = std::exchange(needsScan, false);
  boost::asio::post(scanner, [weak = weak_from_this(), previous = snapshot,
                              changes = std::move(changes), force]() mutable {
    auto self = weak.lock();
    if (!self || self->cancelled) return;
    auto result = watch::update(self->settings, std::move(previous),
                                std::move(changes), force, self->cancelled);
    boost::asio::post(self->io, [weak, result = std::move(result)]() mutable {
      if (auto self = weak.lock(); self && self->running)
        self->scanned(std::move(result));
    });
  });
}

void Watcher::Impl::scanned(std::expected<watch::Update, std::string> result) {
  scanning = false;
  auto applied = result.and_then([&](const auto &update) {
    return update.snapshot == snapshot ? std::expected<void, std::string>{}
                                       : applyScan(*update.snapshot);
  });
  ready = applied.has_value();
  if (!applied) {
    error = applied.error();
    ++failures;
    needsScan = true;
    return;
  }
  snapshot = std::move(result->snapshot);
  ready = snapshot->notices.empty();
  events += result->events;
  if (result->refresh) {
    active = true;
    ++cycles;
    error.clear();
    latestJobs.clear();
    for (auto &command : result->plan.imports) commands.push_back(std::move(command));
    for (auto &command : result->plan.extracts) commands.push_back(std::move(command));
    nextCommand();
  } else if (dirty) {
    changed();
  }
}

void Watcher::Impl::poll() {
  recovery.expires_after(std::chrono::seconds(1));
  recovery.async_wait([weak = weak_from_this()](boost::system::error_code result) {
    if (auto self = weak.lock(); self && self->running && !result) {
      if (!self->dirty || self->needsScan) self->scan();
      self->poll();
    }
  });
}
}
#endif
