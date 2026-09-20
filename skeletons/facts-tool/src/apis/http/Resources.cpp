#include "apis/http/Resources.h"
#include "apis/runtime/Service.h"

namespace facts::apis {
Response resourceError(const domain::Error &error) {
  Json details{{"code", error.code}, {"message", error.message}};
  if (!error.details.is_null()) details["details"] = error.details;
  return response(error.status, {{"error", std::move(details)}});
}
Response Router::resource(const Request &request, const MatchedRoute &route) {
  if (!resources) return error(503, "Domain services unavailable");
  using enum generated::Operation;
  if (route.operation == indexStatus) return response(200, resources->status());
  auto body = Json::parse(request.body(), nullptr, false);
  if (body.is_discarded()) return resourceError({400, "invalid_request", "Malformed JSON"});
  const auto operation = route.operation == extract ? "extract" :
                        route.operation == match ? "match" : "dependencies";
  auto job = runtime::parseRequest(operation, body).and_then([&](auto request) {
    return resources->submit(std::move(request));
  });
  if (!job) return resourceError(job.error());
  auto reply = response(202, *job);
  reply.set(boost::beast::http::field::location,
      endpoint(getJob, job->at("id").get<std::string>()));
  return reply;
}
}
