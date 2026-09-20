#include "apis/jobs/QueueState.h"

namespace facts::apis {
struct Queue::Impl { std::shared_ptr<QueueState> state; };
Queue::Queue(boost::asio::io_context &io, const Settings &settings,
             logging::Logger *logger)
    : impl_(std::make_unique<Impl>(
          Impl{std::make_shared<QueueState>(io, settings, logger)})) {}
Queue::~Queue() { stop(); }
std::optional<std::string> Queue::submit(std::vector<std::string> arguments,
                                       JobCallback completion) {
  return impl_->state->submit(std::move(arguments), std::move(completion));
}
Json Queue::list() const { return impl_->state->list(); }
std::optional<Json> Queue::get(const std::string &id) const {
  return impl_->state->get(id);
}
bool Queue::cancel(const std::string &id) {
  auto state = impl_->state;
  return state->cancel(id);
}
void Queue::pause(bool paused) {
  impl_->state->paused = paused;
  if (!paused) impl_->state->schedule();
}
bool Queue::busy() const { return bool(impl_->state->active); }
void Queue::observe(std::function<void(const Json &)> observer) {
  impl_->state->observer = std::move(observer);
}
void Queue::stop() {
  auto state = impl_->state;
  state->stop();
}
}
