#pragma once
#include "apis/runtime/Request.h"
#include "apis/jobs/Queue.h"

namespace facts::apis::runtime {
using Completion = std::function<void(domain::Result<std::string>)>;
using ResourceWork = std::function<domain::Result<Json>(const domain::Context &)>;
using ResourceCompletion = std::function<void(domain::Result<Json>)>;
using JobReader = std::function<domain::Result<Json>(const Json &, const Json &)>;
struct State;
class Service {
public:
  Service(boost::asio::io_context &, Queue &, const Settings &, logging::Logger *);
  ~Service();
  void start();
  void stop();
  bool busy() const;
  void refresh();
  Json status(bool v2 = false) const;
  void perform(ResourceWork work, ResourceCompletion completion);
  void read(ResourceWork work, ResourceCompletion completion);
  void inspect(const std::string &id, JobReader work, ResourceCompletion completion);
  void updateSettings(const Settings &settings);
  domain::Result<Json> submit(Request request);
  void search(index::Query query, Completion completion);
  Json list() const;
  bool contains(const std::string &id) const;
  void get(const std::string &id, Completion completion);
  domain::Result<void> cancel(const std::string &id);
private:
  std::shared_ptr<State> state_;
};
}
