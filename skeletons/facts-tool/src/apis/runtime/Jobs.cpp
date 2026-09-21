#include "apis/runtime/State.h"
#include <algorithm>
#include <boost/asio/post.hpp>

namespace facts::apis::runtime {
domain::Result<Json> State::submit(Request request, std::optional<std::string> retryOf) {
  if (stopped || !context) return std::unexpected(domain::Error{
      503, "service_not_ready", "Server initialization is incomplete or shutdown has started"});
  const auto queued = std::ranges::count_if(jobs, [](const auto &item) {
    return item.second.at("state") == "queued";
  });
  if (queued >= 64 || work.size() >= 64)
    return std::unexpected(domain::Error{429, "queue_full", "Job queue is full"});
  if (jobs.size() >= 128) {
    auto oldest = std::ranges::find_if(order, [&](const auto &id) {
      return jobs.at(id)["state"] != "queued" && jobs.at(id)["state"] != "running" &&
          jobs.at(id)["state"] != "cancelling";
    });
    if (oldest == order.end()) return std::unexpected(domain::Error{429, "queue_full", "Job retention capacity reached"});
    payloads.erase(*oldest);
    documents.erase(*oldest);
    cancellations.erase(*oldest);
    requests.erase(*oldest);
    jobs.erase(*oldest);
    order.erase(oldest);
  }
  const auto id = "d" + std::to_string(nextId++);
  request.settings = settings;
  requests.emplace(id, request);
  auto cancelled = std::make_shared<std::atomic_bool>(false);
  cancellations.emplace(id, cancelled);
  request.cancelled = [cancelled] { return cancelled->load(); };
  Json job{{"id", id}, {"operation", request.operation}, {"state", "queued"},
      {"created_at", timestamp()}, {"result", nullptr}, {"error", nullptr}};
  if (retryOf) job["retry_of"] = *retryOf;
  jobs.emplace(id, job);
  order.push_back(id);
  log(logging::Level::info, "job.accepted", {{"job_id", id}, {"operation", request.operation}});
  // Post admission so the HTTP handler can produce its accepted response first.
  boost::asio::post(io, [weak = weak_from_this(), id, request = std::move(request)]() mutable {
    if (auto self = weak.lock(); self && !self->stopped)
      self->run(id, std::move(request), *self->context);
  });
  return job;
}
domain::Result<Json> State::retryJob(const std::string &id, const std::string &operation) {
  const auto found = jobs.find(id);
  if (found == jobs.end() || found->second.at("operation") != operation)
    return std::unexpected(domain::Error{404, "job_not_found", "Unknown job for this analysis resource"});
  const auto &state = found->second.at("state");
  if (state != "failed" && state != "cancelled")
    return std::unexpected(domain::Error{
        409, "job_not_retryable", "Only failed or cancelled jobs can be retried"});
  // Copy before admission can evict the oldest retained job. Submission resolves
  // current catalog data and replaces the old cancellation token and settings.
  return submit(requests.at(id), id);
}
Json State::list() const {
  Json result = Json::array();
  for (const auto &id : order) result.push_back(jobs.at(id));
  return result;
}
domain::Result<void> State::cancel(const std::string &id) {
  const auto found = jobs.find(id);
  if (found == jobs.end()) return std::unexpected(domain::Error{404, "job_not_found", "Unknown job"});
  auto &job = found->second;
  if (job["state"] == "running" || job["state"] == "cancelling") {
    if (!job.at("operation").get<std::string>().starts_with("v2."))
      return std::unexpected(domain::Error{
          409, "cannot_cancel_running", "A running native operation must finish safely"});
    cancellations.at(id)->store(true);
    job["state"] = "cancelling";
    return {};
  }
  if (job["state"] == "queued") {
    job["state"] = "cancelled";
    cancellations.at(id)->store(true);
    job["finished_at"] = timestamp();
  }
  return {};
}
}
