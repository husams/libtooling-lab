#include "apis/jobs/QueueState.h"
#include <algorithm>

namespace facts::apis {
bool QueueState::cancel(const std::string &id) {
  auto found = jobs.find(id);
  if (found == jobs.end() || found->second->finished) return false;
  if (found->second == active) {
    auto running = process;
    running->cancel();
    return true;
  }
  auto job = found->second;
  std::erase(pending, job);
  complete(job, ProcessResult{.cancelled = true});
  return true;
}
void QueueState::stop() {
  if (stopped) return;
  stopped = true;
  auto queued = std::move(pending);
  pending.clear();
  for (const auto &job : queued) complete(job, ProcessResult{.cancelled = true});
  if (auto running = process) running->cancel(true);
}
}
