#include "apis/http/Resources.h"
#include "apis/http/Access.h"
#include "apis/runtime/Service.h"

namespace facts::apis {
void Router::handle(const Request &request, Reply reply) {
  const auto method = request.method_string(), target = request.target();
  auto route = authorize(request, settings).and_then([&] {
    return matchRoute({method.data(), method.size()}, {target.data(), target.size()});
  });
  const bool resourceJob = route && resources && resources->contains(route->parameter) &&
      (route->operation == generated::Operation::getJob ||
       route->operation == generated::Operation::cancelJob);
  if (!route || (route->operation != generated::Operation::findSymbols && !resourceJob)) {
    reply((*this)(request));
    return;
  }
  if (!resources) { reply(error(503, "Domain services unavailable")); return; }
  const auto started = std::chrono::steady_clock::now();
  const auto operation = route->operation;
  auto complete = [this, reply = std::move(reply), started, operation,
                   method = std::string(method)](domain::Result<std::string> result) mutable {
    auto response = result ? textResponse(200, std::move(*result), "application/json")
                           : resourceError(result.error());
    if (logger) logger->write(logging::Level::debug, "http.response",
        {{"method", method}, {"route", operationRoute(operation).path},
         {"status", response.result_int()},
         {"duration_ms", std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now() - started).count()}});
    reply(std::move(response));
  };
  if (resourceJob) {
    if (operation == generated::Operation::cancelJob) {
      auto cancelled = resources->cancel(route->parameter);
      if (!cancelled) { complete(std::unexpected(cancelled.error())); return; }
    }
    resources->get(route->parameter, std::move(complete));
    return;
  }
  auto query = runtime::parseQuery({target.data(), target.size()});
  if (!query) { complete(std::unexpected(query.error())); return; }
  resources->search(std::move(*query), std::move(complete));
}
}
