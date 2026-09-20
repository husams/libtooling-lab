#include "apis/watch/State.h"
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
  if (dirty && running) changed();
}
}
