#include "apis/v2/http/Dispatch.h"
#include "apis/http/Resources.h"
#include "apis/http/Access.h"
#include "apis/runtime/Service.h"
#include <set>

namespace facts::apis::v2::http {
void handle(Router &router, const Request &request, Reply reply) {
  auto authorized = authorize(request, router.settings);
  if (!authorized) { reply(resourceError({authorized.error().status, "access_denied", authorized.error().message})); return; }
  auto route = parse(std::string_view(request.target().data(), request.target().size()));
  if (!route) { reply(resourceError(route.error())); return; }
  if ((request.method() == boost::beast::http::verb::get ||
       request.method() == boost::beast::http::verb::delete_) && !request.body().empty()) {
    reply(resourceError({400, "invalid_request", "GET and DELETE requests do not accept a body"})); return;
  }
  if (!router.resources) { reply(resourceError({503, "service_not_ready", "Domain services unavailable"})); return; }
  const std::set<std::string> catalogNames{"repositories", "components", "files", "directories"};
  const std::set<std::string> jobNames{"extract", "match", "import", "dependencies", "callgraphs", "variable-flow", "scan", "index"};
  if (route->jobs && jobNames.contains(route->resource)) { jobs(router, request, *route, std::move(reply)); return; }
  if (route->jobs) { reply(resourceError({404, "not_found", "Unknown job resource"})); return; }
  if (catalogNames.contains(route->resource)) { catalog(router, request, *route, std::move(reply)); return; }
  if (route->resource == "symbols") { symbols(router, request, *route, std::move(reply)); return; }
  if (route->resource == "watcher" || route->resource == "settings") {
    settings(router, request, *route, std::move(reply)); return;
  }
  if (!route->id.empty() || !route->child.empty() || !route->query.empty()) {
    reply(resourceError({404, "not_found", "Unknown API resource"})); return;
  }
  using enum boost::beast::http::verb;
  if (route->resource == "shutdown" && request.method() == post) {
    if (!request.body().empty()) {
      reply(resourceError({400, "invalid_request", "Shutdown does not accept a body"})); return;
    }
    router.shutdown(); reply(response(202, {{"status", "stopping"}})); return;
  }
  const std::set<std::string> names{"health", "readiness", "index", "shutdown"};
  if (!names.contains(route->resource)) { reply(resourceError({404, "not_found", "Unknown API resource"})); return; }
  if (request.method() != get || route->resource == "shutdown") {
    reply(resourceError({405, "method_not_allowed", "Method not supported for this resource"})); return;
  }
  if (route->resource == "health") { reply(response(200, {{"status", "ok"}})); return; }
  if (route->resource == "index") { reply(response(200, router.resources->status(true))); return; }
  const auto index = router.resources->status(true), watch = router.watchStatus();
  const bool indexReady = !index.at("index_revision").is_null();
  const bool watcherReady = !watch.value("enabled", false) || watch.value("ready", false);
  reply(response(indexReady && watcherReady ? 200 : 503,
      {{"ready", indexReady && watcherReady}, {"index_ready", indexReady}, {"watcher_ready", watcherReady}}));
}
}
