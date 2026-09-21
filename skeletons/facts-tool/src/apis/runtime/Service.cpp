#include "apis/runtime/State.h"
#include <boost/asio/post.hpp>

namespace facts::apis::runtime {
Service::Service(boost::asio::io_context &io, Queue &legacy,
                 const Settings &settings, logging::Logger *logger)
    : state_(std::make_shared<State>(io, legacy, settings, logger)) {
  legacy.observe([weak = std::weak_ptr<State>(state_)](const Json &job) {
    const auto &arguments = job.at("arguments");
    if (job.at("state") != "succeeded" || arguments.empty()) return;
    const auto command = arguments.front().get<std::string>();
    if (command != "extract" && command != "match" && command != "import" &&
        command != "dependency") return;
    if (auto state = weak.lock()) state->refresh();
  });
}
Service::~Service() {
  state_->legacy.observe({});
  state_->stop();
  state_->worker.join();
  state_->readers.join();
}
void Service::start() { state_->start(); }
void Service::stop() { state_->stop(); }
bool Service::busy() const { return state_->active || state_->queries != 0; }
void Service::refresh() { state_->refresh(); }
Json Service::status(bool v2) const {
  auto result = state_->indexStatus;
  if (!v2) result.erase("index_revision");
  else if (!result.contains("index_revision")) result["index_revision"] = nullptr;
  return result;
}
void Service::updateSettings(const Settings &settings) { state_->settings = settings; }
void Service::perform(ResourceWork work, ResourceCompletion completion) {
  if (state_->stopped || !state_->context) {
    completion(std::unexpected(domain::Error{503, "service_not_ready", "Server initialization is incomplete"}));
    return;
  }
  if (state_->work.size() >= 64) {
    completion(std::unexpected(domain::Error{429, "queue_full", "Resource queue is full"}));
    return;
  }
  state_->enqueue([state = state_, context = *state_->context,
      work = std::move(work), completion = std::move(completion)]() mutable {
    domain::Result<Json> result;
    try { result = work(context); }
    catch (const std::exception &error) {
      result = std::unexpected(domain::Error{500, "operation_failed", error.what()});
    }
    boost::asio::post(state->io, [completion = std::move(completion),
                               result = std::move(result)]() mutable {
      completion(std::move(result));
    });
  });
}
domain::Result<Json> Service::submit(Request request) {
  return state_->submit(std::move(request));
}
domain::Result<Json> Service::retry(const std::string &id, const std::string &operation) {
  return state_->retryJob(id, operation);
}
void Service::search(index::Query query, Completion completion) {
  state_->search(std::move(query), std::move(completion));
}
Json Service::list() const { return state_->list(); }
bool Service::contains(const std::string &id) const { return state_->jobs.contains(id); }
void Service::get(const std::string &id, Completion completion) {
  state_->get(id, std::move(completion));
}
domain::Result<void> Service::cancel(const std::string &id) {
  return state_->cancel(id);
}
}
