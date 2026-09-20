#include "apis/v2/http/Dispatch.h"
#include "apis/http/Resources.h"
#include <set>

namespace facts::apis::v2::http {
Json watchSettings(const Settings &value) {
  return {{"enabled", value.watchEnabled}, {"debounce_ms", value.debounceMs},
      {"exclude_repositories", value.excludedRepositories}, {"exclude_clones", value.excludedClones},
      {"exclude_directories", value.excludedDirectories}, {"exclude_patterns", value.excludePatterns}};
}
domain::Result<Settings> patchSettings(const Settings &current, const Json &body, bool replace) {
  auto expected = watchSettings(current);
  if (replace && body.size() != expected.size())
    return std::unexpected(domain::Error{422, "invalid_request", "PUT requires all watcher settings"});
  auto result = current;
  for (const auto &[key, value] : body.items()) {
    if (!expected.contains(key)) return std::unexpected(domain::Error{422, "invalid_request", "Unknown watcher setting: " + key});
    if (key == "enabled") {
      if (!value.is_boolean()) return std::unexpected(domain::Error{422, "invalid_request", "enabled must be boolean"});
      result.watchEnabled = value.get<bool>();
    } else if (key == "debounce_ms") {
      if (!value.is_number_integer() || value < 1 || value > 60000)
        return std::unexpected(domain::Error{422, "invalid_request", "debounce_ms must be between 1 and 60000"});
      result.debounceMs = value.get<unsigned>();
    } else {
      if (!value.is_array() || value.size() > 4096)
        return std::unexpected(domain::Error{422, "invalid_request", key + " must be an array"});
      for (const auto &item : value)
        if (!item.is_string() || item.get_ref<const std::string &>().empty() ||
            item.get_ref<const std::string &>().size() > 4096 ||
            item.get_ref<const std::string &>().find('\0') != std::string::npos)
          return std::unexpected(domain::Error{422, "invalid_request", key + " contains an invalid string"});
      auto *target = key == "exclude_repositories" ? &result.excludedRepositories :
          key == "exclude_clones" ? &result.excludedClones :
          key == "exclude_directories" ? &result.excludedDirectories : &result.excludePatterns;
      *target = value.get<std::vector<std::string>>();
    }
  }
  return result;
}
void settings(Router &router, const Request &request, const Route &route, Reply reply) {
  using enum boost::beast::http::verb;
  if (!route.query.empty()) { reply(resourceError({400, "invalid_query", "Settings accept no query parameters"})); return; }
  if (route.resource == "settings" && route.id.empty() && request.method() == get) {
    reply(response(200, {{"watcher", watchSettings(router.settings)},
        {"timeout_seconds", router.settings.timeoutSeconds}, {"authenticated", !router.settings.token.empty()}})); return;
  }
  if (route.resource == "watcher" && route.id.empty() && request.method() == get) {
    auto result = router.watchStatus(); result.erase("project_database");
    for (auto &clone : result["clones"])
      for (const auto *key : {"repository_id", "clone_id"})
        if (clone.contains(key) && clone[key].is_number_integer())
          clone[key] = std::to_string(clone[key].get<std::int64_t>());
    reply(response(200, result)); return;
  }
  if (route.resource != "watcher" || route.id != "settings" || !route.child.empty()) {
    reply(resourceError({404, "not_found", "Unknown settings resource"})); return;
  }
  if (request.method() == get) { reply(response(200, watchSettings(router.settings))); return; }
  if (request.method() != put && request.method() != patch) {
    reply(resourceError({405, "method_not_allowed", "Use GET, PUT or PATCH"})); return;
  }
  if (!router.updateWatch) { reply(resourceError({503, "service_unavailable", "Watcher reconfiguration unavailable"})); return; }
  auto result = body(request).and_then([&](const Json &value) { return router.updateWatch(value, request.method() == put); });
  reply(result ? response(200, *result) : resourceError(result.error()));
}
}
