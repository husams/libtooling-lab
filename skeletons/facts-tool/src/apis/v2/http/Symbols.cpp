#include "apis/v2/http/Dispatch.h"
#include "apis/v2/symbols/Symbols.h"
#include "apis/http/Resources.h"
#include "apis/runtime/Service.h"

namespace facts::apis::v2::http {
void symbols(Router &router, const Request &request, const Route &route, Reply reply) {
  if (request.method() != boost::beast::http::verb::get) {
    reply(resourceError({405, "method_not_allowed", "Symbols are read-only resources"})); return;
  }
  if (route.id.empty() && !route.child.empty()) {
    reply(resourceError({404, "not_found", "Unknown symbol resource"})); return;
  }
  if (router.resources->status(true).at("index_revision").is_null()) {
    reply(resourceError({503, "index_not_ready", "The global symbol index is not ready"})); return;
  }
  router.resources->read([route](const auto &context) -> domain::Result<Json> {
    v2::symbols::QueryParameters query;
    for (const auto &[key, value] : route.query.items()) query[key] = value.template get<std::string>();
    return route.id.empty() ? v2::symbols::search(context, query) :
        v2::symbols::read(context, route.id, route.child, query);
  }, [reply = std::move(reply)](auto result) mutable {
    reply(result ? response(200, *result) : resourceError(result.error()));
  });
}
}
