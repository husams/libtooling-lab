#include "apis/watch/State.h"
#include "apis/watch/Snapshot.h"
#include <chrono>

namespace facts::apis {
void Watcher::Impl::changed() {
  dirty = true;
  debounce.expires_after(std::chrono::milliseconds(settings.debounceMs));
  debounce.async_wait([weak = weak_from_this()](boost::system::error_code result) {
    if (auto self = weak.lock(); self && self->running && !result)
      self->refresh();
  });
}

void Watcher::Impl::refresh() {
  if (active || scanning || !dirty || !running) return;
#ifdef __linux__
  scan();
#endif
}

void Watcher::Impl::nextCommand() {
  if (!running) return;
  if (commands.empty()) { finished(true); return; }
  auto command = std::move(commands.front());
  commands.pop_front();
  const auto name = command.front();
  auto id = queue.submit(std::move(command), [weak = weak_from_this(), name](bool ok) {
    if (auto self = weak.lock(); self && self->running) {
      if (ok) self->nextCommand();
      else {
        self->error = "watch " + name + " failed; inspect latest_jobs";
        self->finished(false);
      }
    }
  });
  if (id) latestJobs.push_back(*id);
  else { error = "job queue is full or stopped"; finished(false); }
}

void Watcher::Impl::finished(bool success) {
  active = false;
  commands.clear();
  if (!success) { ++failures; needsScan = true; }
  checkpointPending = success;
  if (logger) logger->write(success ? logging::Level::debug : logging::Level::warning,
      "watch.cycle.completed", {{"cycle", cycles}, {"succeeded", success},
                                {"jobs", latestJobs.size()}, {"failures", failures}});
#ifdef __linux__
  // Re-read stable compilation metadata after successful import before making
  // its pre-processing snapshot reusable. A changed catalog triggers another
  // normal cycle; an unchanged catalog publishes the pending checkpoint.
  if (success && running) { scan(); return; }
#endif
  if (dirty && running) changed();
}
void Watcher::Impl::checkpoint() {
  checkpointPending = false;
  if (!snapshot) return;
  auto saved = watch::remember(settings, *snapshot);
  if (!saved && logger) logger->write(logging::Level::warning, "watch.checkpoint.failed",
                                     {{"message", saved.error()}});
}
}
