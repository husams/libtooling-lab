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
    if (request.operation == "v2.extract" || request.operation == "v2.match" ||
        request.operation == "v2.import" || request.operation == "v2.dependencies" ||
        request.operation == "v2.index") {
      try {
      auto published = domain::factSources(context).and_then([&](const auto &sources) {
        return index::refresh(context.configuration.database, sources,
            context.configuration.projectRoot).transform_error([](const auto &message) {
          return domain::Error{503, "index_failed", message};
        });
      });
      if (published) {
        if (result) (*result)["index_revision"] = std::to_string(published->generation);
        boost::asio::post(self->io, [weak, published = std::move(published)]() mutable {
          if (auto state = weak.lock()) state->indexed(std::move(published));
        });
      } else if (result) result = std::unexpected(published.error());
      } catch (const std::exception &error) {
        if (result) result = std::unexpected(domain::Error{503, "index_failed", error.what()});
      }
    }
    if (result && request.operation == "v2.scan" && result->contains("warnings"))
      for (auto warning : result->at("warnings")) {
        warning["job_id"] = id;
        self->log(logging::Level::warning, "scan.warning", std::move(warning));
      }
    if (request.operation == "v2.import") {
      const auto &details = result ? *result : result.error().details;
      if (details.is_object() && details.contains("diagnostics"))
        for (auto diagnostic : details.at("diagnostics"))
          if (diagnostic.value("severity", "") == "warning") {
            diagnostic["job_id"] = id;
            self->log(logging::Level::warning, "import.warning", std::move(diagnostic));
          }
    }
    const bool success = result.has_value();
    Json failure = result ? Json(nullptr) : encodeError(result.error());
    Json document{{"result", result ? std::move(*result) : Json(nullptr)},
                  {"error", std::move(failure)}};
    const bool nativeV2 = request.operation.starts_with("v2.");
    auto payload = nativeV2 ? nullptr : std::make_shared<const std::string>(serialize(document));
    auto retained = nativeV2 ? std::make_shared<const Json>(std::move(document)) : nullptr;
    Json summary = success ? Json(nullptr) : Json{
        {"code", result.error().code}, {"message", result.error().message}};
    boost::asio::post(self->io, [weak, id, success, error = std::move(summary),
                                payload = std::move(payload), retained = std::move(retained)]() mutable {
      if (auto self = weak.lock())
        self->complete(id, success, std::move(error), std::move(payload), std::move(retained));
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
                     std::shared_ptr<const std::string> payload,
                     std::shared_ptr<const Json> document) {
  auto &job = jobs.at(id);
  const bool cancelled = !success && error.is_object() &&
      error.value("code", "") == "cancelled";
  job["state"] = success ? "succeeded" : cancelled ? "cancelled" : "failed";
  job["finished_at"] = timestamp();
  job["error"] = std::move(error);
  payloads[id] = std::move(payload);
  if (document) documents[id] = std::move(document);
  log(success ? logging::Level::info : logging::Level::error,
      "job.completed", {{"job_id", id}, {"state", job["state"]}});
  if (success && !stopped && !job.at("operation").get<std::string>().starts_with("v2.")) refresh();
}
}
