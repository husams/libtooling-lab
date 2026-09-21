#include "apis/v2/http/Dispatch.h"
#include "apis/v2/Jobs.h"
#include "apis/http/Resources.h"
#include "apis/runtime/Service.h"
#include "apis/runtime/Validation.h"
#include <set>

namespace facts::apis::v2::http {
namespace {
Json publicJob(Json job) {
  auto operation = job.at("operation").get<std::string>();
  if (operation.starts_with("v2.")) job["operation"] = operation.substr(3);
  return job;
}
std::string membership(const Json &items, const std::string &resource) {
  const auto prefix = resource + ":jobs:";
  if (items.empty()) return prefix + "empty";
  // IDs increase monotonically and retention only removes jobs. Binding the
  // boundaries and count detects membership changes without expiring cursors
  // when an existing job changes from queued to running or terminal.
  return prefix + items.front().at("id").get<std::string>() + ":" +
      items.back().at("id").get<std::string>() + ":" + std::to_string(items.size());
}
domain::Result<Json> results(const Json &job, const Json &result, const Route &route) {
  if (job["state"] != "succeeded") return std::unexpected(domain::Error{
      409, "results_unavailable", "Results are available after the job succeeds"});
  const std::string fallback = route.resource == "match" ? "matches" :
      route.resource == "scan" ? "warnings" :
      route.resource == "extract" ? "files" : route.resource == "import" ? "databases" : "edges";
  if (route.resource == "index") return page(Json::array({result}), route.query, route.id + ":index");
  const auto collection = route.query.value("collection", fallback);
  const std::map<std::string, std::set<std::string>> allowed{
      {"extract", {"files", "diagnostics"}}, {"match", {"matches", "diagnostics"}},
      {"dependencies", {"edges", "diagnostics"}}, {"import", {"databases", "diagnostics"}},
      {"scan", {"warnings", "databases", "diagnostics"}},
      {"callgraphs", {"nodes", "edges", "paths", "frontier", "diagnostics"}},
      {"variable-flow", {"nodes", "edges", "boundaries", "diagnostics"}}};
  if (!allowed.at(route.resource).contains(collection))
    return std::unexpected(domain::Error{400, "invalid_collection", "Unknown result collection"});
  const auto empty = Json::array();
  return page(result.contains(collection) ? result.at(collection) : empty,
      route.query, route.resource + ":" + route.id + ":" + collection);
}
Json summary(const Json &metadata, const Json &document) {
  auto job = publicJob(metadata);
  if (!document.is_object()) return job;
  job["error"] = document.at("error");
  if (document.at("result").is_object()) {
    Json result = Json::object();
    for (const auto &[key, value] : document.at("result").items())
      if (!value.is_array()) result[key] = value;
    job["result"] = std::move(result);
  }
  return job;
}
}
void jobs(Router &router, const Request &request, const Route &route, Reply reply) {
  using enum boost::beast::http::verb;
  if (!route.child.empty() && route.child != "results") {
    reply(resourceError({404, "not_found", "Unknown job resource"})); return;
  }
  if (!route.id.empty() && route.child.empty() && !route.query.empty()) {
    reply(resourceError({400, "invalid_query", "Individual jobs accept no query parameters"})); return;
  }
  for (const auto &[key, value] : route.query.items())
    if (key != "limit" && key != "cursor" && !(key == "collection" && route.child == "results")) {
      reply(resourceError({400, "invalid_query", "Unknown query parameter"})); return;
    }
  if (route.id.empty() && request.method() == post) {
    if (!route.query.empty()) { reply(resourceError({400, "invalid_query", "Job creation has no query parameters"})); return; }
    auto submitted = body(request).and_then([&](const Json &value) -> domain::Result<Json> {
      if (value.contains("retry_of")) {
        return runtime::keys(value, {"retry_of"}).and_then([&] {
          return runtime::text(value.at("retry_of"), "retry_of");
        }).and_then([&](const auto &id) {
          return router.resources->retry(id, "v2." + route.resource);
        });
      }
      return parseJobRequest(route.resource, value).and_then([&](auto parsed) {
        return router.resources->submit(std::move(parsed));
      });
    });
    if (!submitted) { reply(resourceError(submitted.error())); return; }
    auto response = facts::apis::response(202, publicJob(*submitted));
    response.set(boost::beast::http::field::location, "/api/v2/" + route.resource + "/job/" + submitted->at("id").get<std::string>());
    reply(std::move(response)); return;
  }
  if (route.id.empty() && request.method() == get) {
    Json items = Json::array();
    for (auto job : router.resources->list())
      if (job["operation"] == "v2." + route.resource) items.push_back(publicJob(std::move(job)));
    auto result = page(items, route.query, membership(items, route.resource));
    reply(result ? response(200, *result) : resourceError(result.error())); return;
  }
  if (route.id.empty() || (request.method() != get && request.method() != delete_) ||
      (!route.child.empty() && request.method() != get)) {
    reply(resourceError({405, "method_not_allowed", "Method not supported for this job resource"})); return;
  }
  router.resources->inspect(route.id, [route](const Json &metadata, const Json &document) -> domain::Result<Json> {
    if (metadata["operation"] != "v2." + route.resource)
      return std::unexpected(domain::Error{404, "job_not_found", "Unknown job"});
    const Json empty = nullptr;
    if (route.child == "results")
      return results(metadata, document.is_object() ? document.at("result") : empty, route);
    return summary(metadata, document);
  }, [&router, route, method = request.method(), reply = std::move(reply)](auto value) mutable {
    if (!value) { reply(resourceError(value.error())); return; }
    if (method == delete_) {
      auto cancelled = router.resources->cancel(route.id);
      if (!cancelled) { reply(resourceError(cancelled.error())); return; }
      router.resources->inspect(route.id, summary, [reply = std::move(reply)](auto current) mutable {
        if (!current) { reply(resourceError(current.error())); return; }
        auto &job = *current;
        reply(response(job["state"] == "cancelling" ? 202 : 200, job));
      }); return;
    }
    reply(response(200, *value));
  });
}
}
