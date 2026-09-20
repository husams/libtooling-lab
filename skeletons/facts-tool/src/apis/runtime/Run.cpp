#include "apis/runtime/State.h"
#include <boost/asio/post.hpp>

namespace facts::apis::runtime {
void State::run(std::string id, Request request, domain::Context context) {
  if (!jobs.contains(id) || jobs.at(id)["state"] == "cancelled") return;
  enqueue([weak = weak_from_this(), id, request = std::move(request), context] {
    const auto self = weak.lock();
    if (!self) return;
    domain::Result<Json> result;
    try { result = execute(context, request); }
    catch (const std::exception &error) {
      result = std::unexpected(domain::Error{500, "operation_failed", error.what()});
    }
    const bool success = result.has_value();
    Json failure = result ? Json(nullptr) : encodeError(result.error());
    Json document{{"result", result ? std::move(*result) : Json(nullptr)},
                  {"error", std::move(failure)}};
    auto payload = std::make_shared<const std::string>(serialize(document));
    Json summary = success ? Json(nullptr) : Json{
        {"code", result.error().code}, {"message", result.error().message}};
    boost::asio::post(self->io, [weak, id, success, error = std::move(summary),
                                payload = std::move(payload)]() mutable {
      if (auto self = weak.lock())
        self->complete(id, success, std::move(error), std::move(payload));
    });
  }, [weak = weak_from_this(), id] {
    const auto self = weak.lock();
    if (!self || self->stopped || !self->jobs.contains(id) ||
        self->jobs.at(id)["state"] == "cancelled") return false;
    self->jobs.at(id)["state"] = "running";
    self->jobs.at(id)["started_at"] = timestamp();
    self->log(logging::Level::info, "job.started", {{"job_id", id}});
    return true;
  });
}
void State::complete(std::string id, bool success, Json error,
                     std::shared_ptr<const std::string> payload) {
  auto &job = jobs.at(id);
  job["state"] = success ? "succeeded" : "failed";
  job["finished_at"] = timestamp();
  job["error"] = std::move(error);
  payloads[id] = std::move(payload);
  log(success ? logging::Level::info : logging::Level::error,
      "job.completed", {{"job_id", id}, {"state", job["state"]}});
  if (success && !stopped) refresh();
}
}
