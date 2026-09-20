#include "apis/watch/State.h"
#include "apis/watch/Arguments.h"
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
  dirty = false;
  active = true;
  latestJobs.clear();
  error.clear();
  ++cycles;
#ifdef __linux__
  if (needsScan) {
    scan();
    return;
  }
#endif
  prepareImports();
  importNext();
}

void Watcher::Impl::importNext() {
  if (!running) return;
  if (imports.empty()) { extract(); return; }
  auto command = std::move(imports.front());
  imports.pop_front();
  auto id = queue.submit(std::move(command), [weak = weak_from_this()](bool ok) {
    if (auto self = weak.lock(); self && self->running) {
      if (ok) self->importNext();
      else { self->error = "watch import failed; inspect latest_jobs";
             self->finished(false); }
    }
  });
  if (id) latestJobs.push_back(*id);
  else { error = "job queue is full or stopped"; finished(false); }
}

void Watcher::Impl::extract() {
  auto values = settings.extractArguments;
  watch::enableFlag(values, "--force");
  auto id = queue.submit(arguments("extract", std::move(values)),
      [weak = weak_from_this()](bool ok) {
        if (auto self = weak.lock(); self && self->running) {
          if (!ok) self->error = "watch extraction failed; inspect latest_jobs";
          self->finished(ok);
        }
      });
  if (id) latestJobs.push_back(*id);
  else { error = "job queue is full or stopped"; finished(false); }
}

void Watcher::Impl::finished(bool success) {
  active = false;
  if (!success) ++failures;
  if (dirty && running) changed();
}
}
