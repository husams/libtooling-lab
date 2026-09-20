#include "apis/runtime/State.h"

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
Json Service::status() const { return state_->indexStatus; }
domain::Result<Json> Service::submit(Request request) {
  return state_->submit(std::move(request));
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
