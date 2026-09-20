#include "apis/v2/http/Dispatch.h"
#include "apis/v2/catalog/Catalog.h"
#include "apis/http/Resources.h"
#include "apis/runtime/Service.h"

namespace facts::apis::v2::http {
void catalog(Router &router, const Request &request, const Route &route, Reply reply) {
  if (!route.child.empty()) { reply(resourceError({404, "not_found", "Unknown catalog resource"})); return; }
  auto input = body(request);
  if (!input) { reply(resourceError(input.error())); return; }
  router.resources->perform([route, input = *input, method = std::string(request.method_string())](const auto &context) {
    return v2::catalog::dispatch(context, method, route.resource, route.id, input, route.query)
      .transform([](auto result) { return Json{{"status", result.status}, {"body", result.body},
          {"location", result.location}, {"changed", result.changed}}; });
  }, [&router, reply = std::move(reply)](auto result) mutable {
    if (!result) { reply(resourceError(result.error())); return; }
    if (result->at("changed")) {
      router.resources->refresh();
      if (router.reconcile) router.reconcile();
    }
    const auto status = result->at("status").template get<unsigned>();
    auto answer = status == 204 ? textResponse(204, "", "application/json")
                                : response(status, result->at("body"));
    const auto location = result->at("location").template get<std::string>();
    if (!location.empty()) answer.set(boost::beast::http::field::location, location);
    reply(std::move(answer));
  });
}
}
