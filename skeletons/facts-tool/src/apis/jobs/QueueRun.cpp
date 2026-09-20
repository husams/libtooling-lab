#include "apis/jobs/QueueState.h"
#include <boost/asio/post.hpp>

namespace facts::apis {
void QueueState::schedule() {
  boost::asio::post(io, [weak = weak_from_this()] {
    if (auto state = weak.lock()) state->pump();
  });
}
void QueueState::pump() {
  if (stopped || active || pending.empty()) return;
  active = pending.front();
  pending.pop_front();
  active->record["state"] = "running";
  active->record["started_at"] = jobTimestamp();
  if (logger) logger->write(logging::Level::info, "job.started",
                           {{"job_id", active->record["id"]}});
  process = std::make_shared<Process>(io, settings.executable,
      active->record["arguments"].get<std::vector<std::string>>(),
      settings.timeoutSeconds, [weak = weak_from_this(), job = active](auto result) {
    if (auto state = weak.lock()) state->complete(job, std::move(result));
  });
  auto running = process;
  running->start();
}
void QueueState::complete(const std::shared_ptr<Job> &job, ProcessResult result) {
  if (job->finished) return;
  const bool success = !result.cancelled && !result.timedOut && result.exitCode == 0;
  job->record["state"] = result.cancelled ? "cancelled" : success ? "succeeded" : "failed";
  job->record["exit_code"] = result.exitCode;
  job->record["stdout"] = std::move(result.output);
  job->record["stderr"] = std::move(result.error);
  job->record["truncated"] = result.truncated;
  job->record["timed_out"] = result.timedOut;
  job->record["finished_at"] = jobTimestamp();
  job->finished = true;
  const auto level = result.timedOut ? logging::Level::warning :
      (success || result.cancelled) ? logging::Level::info : logging::Level::error;
  if (logger) logger->write(level, "job.completed",
      {{"job_id", job->record["id"]}, {"state", job->record["state"]},
       {"exit_code", result.exitCode}, {"timed_out", result.timedOut},
       {"truncated", result.truncated}});
  if (active == job) { active.reset(); process.reset(); }
  auto completion = std::move(job->completion);
  schedule();
  if (completion) completion(success);
}
}
