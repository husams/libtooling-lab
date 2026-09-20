#include "apis/jobs/QueueState.h"
#include <algorithm>

namespace facts::apis {
std::optional<std::string> QueueState::submit(std::vector<std::string> arguments,
                                            JobCallback completion) {
  if (stopped || pending.size() >= 64 || arguments.empty()) {
    if (logger) logger->write(logging::Level::warning, "queue.rejected",
        {{"reason", stopped ? "stopping" : arguments.empty() ? "empty" : "capacity"}});
    return std::nullopt;
  }
  if (std::any_of(arguments.begin(), arguments.end(), [](const auto &argument) {
        return argument.find('\0') != std::string::npos;
      })) return std::nullopt;
  if (jobs.size() >= 128) {
    auto oldest = std::find_if(order.begin(), order.end(), [this](const auto &id) {
      return jobs.at(id)->finished;
    });
    if (oldest == order.end()) {
      if (logger) logger->write(logging::Level::warning, "queue.rejected",
                               {{"reason", "retention_capacity"}});
      return std::nullopt;
    }
    jobs.erase(*oldest);
    order.erase(oldest);
  }
  auto id = std::to_string(nextId++);
  auto job = std::make_shared<Job>(Job{
      {{"id", id}, {"state", "queued"}, {"arguments", std::move(arguments)},
       {"exit_code", nullptr}, {"stdout", ""}, {"stderr", ""},
       {"truncated", false}, {"timed_out", false}, {"created_at", jobTimestamp()}},
      std::move(completion)});
  jobs.emplace(id, job);
  order.push_back(id);
  pending.push_back(std::move(job));
  if (logger) logger->write(logging::Level::info, "job.accepted",
                           {{"job_id", id}, {"queued", pending.size()}});
  schedule();
  return id;
}
Json QueueState::list() const {
  Json result = Json::array();
  for (const auto &id : order) {
    Json record = Json::object();
    for (const auto &[name, value] : jobs.at(id)->record.items())
      if (name != "stdout" && name != "stderr") record[name] = value;
    result.push_back(std::move(record));
  }
  return result;
}
std::optional<Json> QueueState::get(const std::string &id) const {
  auto found = jobs.find(id);
  if (found == jobs.end()) return std::nullopt;
  return found->second->record;
}
}
