#include "apis/watch/State.h"
#include "apis/watch/Snapshot.h"
#include <chrono>

namespace facts::apis {
namespace {
std::string excerpt(const std::string &text, std::size_t limit) {
  if (text.size() <= limit) return text;
  return text.substr(0, limit / 2) + "\n[diagnostics truncated; inspect job output]\n" +
         text.substr(text.size() - limit / 2);
}

std::string failureDetails(const std::string &name, const Json &job,
                           const watch::Scan *snapshot) {
  std::string message = "watch " + name + " failed; job=" + job.value("id", "unknown");
  if (snapshot) {
    Json clones = Json::array();
    for (const auto &clone : snapshot->catalog.clones)
      if (clone.active && clone.excluded.empty())
        clones.push_back({{"repository", clone.repository}, {"clone", clone.label},
                          {"path", clone.path.string()}});
    message += "; active_clones=" + excerpt(clones.dump(), 2048);
  }
  message += "; arguments=" + excerpt(job.value("arguments", Json::array()).dump(), 4096);
  const auto diagnostics = job.value("stderr", "");
  message += "\n" + excerpt(diagnostics.empty() ? job.value("stdout", "") : diagnostics, 8192);
  return message;
}
}

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
        const auto job = self->latestJobs.empty() ? std::nullopt
            : self->queue.get(self->latestJobs.back());
        self->error = failureDetails(name, job.value_or(Json::object()), self->snapshot.get());
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
  // Failed inputs remain observable through last_error/latest_jobs. Repeating
  // the same command every poll cannot repair missing headers or compiler
  // options; a relevant filesystem/catalog change or reconcile schedules retry.
  if (!success) ++failures;
  checkpointPending = success;
  if (logger) logger->write(success ? logging::Level::debug : logging::Level::warning,
      "watch.cycle.completed", {{"cycle", cycles}, {"succeeded", success},
                                {"jobs", latestJobs.size()}, {"failures", failures},
                                {"message", excerpt(error, 4096)}});
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
