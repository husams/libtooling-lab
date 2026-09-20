#pragma once
#include "apis/jobs/Job.h"
#include "apis/jobs/Process.h"
#include "apis/logging/Logger.h"
#include <deque>
#include <unordered_map>

namespace facts::apis {
struct QueueState : std::enable_shared_from_this<QueueState> {
  QueueState(boost::asio::io_context &context, const Settings &configuration,
             logging::Logger *logger)
      : io(context), settings(configuration), logger(logger) {}
  std::optional<std::string> submit(std::vector<std::string>, JobCallback);
  Json list() const;
  std::optional<Json> get(const std::string &) const;
  bool cancel(const std::string &);
  void stop();
  void pump();
  void complete(const std::shared_ptr<Job> &, ProcessResult);
  void schedule();
  boost::asio::io_context &io;
  Settings settings;
  logging::Logger *logger;
  std::unordered_map<std::string, std::shared_ptr<Job>> jobs;
  std::deque<std::string> order;
  std::deque<std::shared_ptr<Job>> pending;
  std::shared_ptr<Process> process;
  std::shared_ptr<Job> active;
  std::uint64_t nextId = 1;
  bool stopped = false;
};
}
