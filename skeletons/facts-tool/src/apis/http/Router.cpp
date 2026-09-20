#include "apis/http/Router.h"
#include "apis/http/Access.h"
#include <chrono>

namespace facts::apis {
Response Router::operator()(const Request &request) {
  const auto started = std::chrono::steady_clock::now();
  const auto method = request.method_string(), path = request.target();
  const auto route = authorize(request, settings).and_then([&] {
    return matchRoute({method.data(), method.size()}, {path.data(), path.size()});
  });
  auto reply = route ? dispatch(request, *route)
                     : error(route.error().status, route.error().message);
  const auto elapsed = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - started).count();
  const auto safeMethod = request.method() == boost::beast::http::verb::unknown
      ? "UNKNOWN" : boost::beast::http::to_string(request.method());
  if (logger) logger->write(logging::Level::debug, "http.response",
      {{"method", std::string(safeMethod)}, {"status", reply.result_int()},
       {"route", route ? operationRoute(route->operation).path : "unmatched"},
       {"duration_ms", elapsed}});
  return reply;
}
Response Router::dispatch(const Request &request, const MatchedRoute &route) {
  using enum generated::Operation;
  switch (route.operation) {
  case openapi: return textResponse(200, documents.json, "application/json");
  case openapiYaml: return textResponse(200, documents.yaml, "application/yaml");
  case health: return response(200, {{"status", "ok"}});
  case commands: {
    Json catalog = Json::array();
    for (const auto &path : this->commands)
      catalog.push_back({{"path", path}, {"endpoint", endpoint(command, path)}});
    return response(200, {{"commands", catalog}});
  }
  case watchStatus: return response(200, this->watchStatus());
  case shutdown:
    this->shutdown();
    return response(202, {{"status", "stopping"}});
  case listJobs: return response(200, {{"jobs", jobs.list()}});
  case submit: return this->submit(request, "");
  case command: return this->submit(request, route.parameter);
  case getJob:
  case cancelJob: {
    const auto job = jobs.get(route.parameter);
    if (!job) return error(404, "Unknown job");
    if (route.operation == getJob) return response(200, *job);
    jobs.cancel(route.parameter);
    return response(200, *jobs.get(route.parameter));
  }
  }
  return error(500, "Operation has no handler");
}
}
