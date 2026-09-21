#include "apis/watch/State.h"
#include "apis/watch/Snapshot.h"
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
  if (logger) logger->write(logging::Level::trace, "watch.scan",
      {{"changes", changes.size()}, {"forced", force}});
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
  const bool previouslyReady = ready;
  ready = applied.has_value();
  if (!applied) {
    checkpointPending = false;
    if (previouslyReady || error != applied.error()) {
      ++failures;
      if (logger) logger->write(logging::Level::warning, "watch.failed",
          {{"stage", "scan"}, {"message", applied.error()}});
    }
    error = applied.error();
    needsScan = true;
    return;
  }
  snapshot = std::move(result->snapshot);
  ready = snapshot->notices.empty();
  events += result->events;
  if (logger) logger->write(logging::Level::debug, "watch.scanned",
      {{"ready", ready}, {"notices", snapshot->notices.size()},
       {"events", result->events}, {"refresh", result->refresh}});
  if (result->refresh) {
    checkpointPending = false;
    if (!result->plan) {
      error = result->plan.error();
      ++failures;
      if (logger) logger->write(logging::Level::warning, "watch.failed",
          {{"stage", "plan"}, {"message", error}});
      if (dirty) changed();
      return;
    }
    auto invalidated = watch::forget(settings);
    if (!invalidated) {
      error = invalidated.error();
      ready = false;
      ++failures;
      needsScan = true;
      return;
    }
    resumed = false;
    active = true;
    ++cycles;
    error.clear();
    latestJobs.clear();
    for (auto &command : result->plan->imports) commands.push_back(std::move(command));
    for (auto &command : result->plan->extracts) commands.push_back(std::move(command));
    if (logger) logger->write(logging::Level::debug, "watch.cycle.started",
        {{"cycle", cycles}, {"commands", commands.size()}});
    nextCommand();
  } else if (dirty) {
    changed();
  } else if (checkpointPending && !needsScan && pendingEvents.empty()) {
    checkpoint();
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
