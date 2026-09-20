#include "apis/runtime/State.h"
#include <boost/asio/post.hpp>

namespace facts::apis::runtime {
State::State(boost::asio::io_context &io, Queue &legacy, const Settings &settings,
             logging::Logger *logger)
    : io(io), legacy(legacy), settings(settings), logger(logger), retry(io) {}
void State::log(logging::Level level, std::string_view event, Json fields) {
  if (logger) logger->write(level, event, std::move(fields));
}
void State::enqueue(std::function<void()> task, std::function<bool()> admit) {
  work.push_back({std::move(task), std::move(admit)});
  pump();
}
void State::pump() {
  if (active || stopped || work.empty()) return;
  legacy.pause(true);
  if (legacy.busy()) {
    retry.expires_after(std::chrono::milliseconds(20));
    retry.async_wait([weak = weak_from_this()](auto error) {
      if (auto self = weak.lock(); self && !error) self->pump();
    });
    return;
  }
  auto task = std::move(work.front());
  work.pop_front();
  if (task.admit && !task.admit()) {
    if (work.empty()) legacy.pause(false);
    pump();
    return;
  }
  active = true;
  boost::asio::post(worker, [weak = weak_from_this(), task = std::move(task)] {
    task.work();
    if (auto self = weak.lock()) boost::asio::post(self->io, [weak] {
      if (auto self = weak.lock()) {
        self->active = false;
        self->legacy.pause(false);
        self->pump();
      }
    });
  });
}
void State::stop() {
  if (stopped) return;
  stopped = true;
  retry.cancel();
  work.clear();
  for (auto &[id, job] : jobs) if (job["state"] == "queued") {
    job["state"] = "cancelled";
    job["finished_at"] = timestamp();
  }
  legacy.pause(false);
}
std::int64_t timestamp() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
}
}
