#include "apis/http/Router.h"
#include "apis/http/OpenApi.h"

namespace facts::apis {
Response Router::operator()(const Request &request) {
  namespace http = boost::beast::http;
  if (request.find(http::field::origin) != request.end())
    return error(403, "Browser-origin requests are not supported");
  if (request.find("Sec-Fetch-Site") != request.end())
    return error(403, "Browser requests are not supported");
  if (settings.token.empty()) {
    const auto address = settings.host.find(':') == std::string::npos
        ? settings.host : "[" + settings.host + "]";
    const auto host = request[http::field::host];
    const auto port = ":" + std::to_string(settings.port);
    if (host != address && host != address + port &&
        host != "localhost" && host != "localhost" + port)
      return error(403, "Host must match the local listener address");
  }
  if (!settings.token.empty() &&
      request[http::field::authorization] != "Bearer " + settings.token)
    return error(401, "Bearer authentication required");
  const std::string path(request.target());
  const auto method = request.method();
  if (path == "/openapi.json")
    return method == http::verb::get
        ? response(200, openApi(commands, !settings.token.empty()))
        : error(405, "Use GET");
  if (path == "/health")
    return method == http::verb::get
        ? response(200, {{"status", "ok"}}) : error(405, "Use GET");
  if (path == "/v1/commands") {
    if (method != http::verb::get) return error(405, "Use GET");
    Json catalog = Json::array();
    for (const auto &command : commands)
      catalog.push_back({{"path", command},
                         {"endpoint", "/v1/commands/" + command}});
    return response(200, {{"commands", catalog}});
  }
  if (path == "/v1/watch")
    return method == http::verb::get ? response(200, watchStatus())
                                     : error(405, "Use GET");
  if (path == "/v1/shutdown") {
    if (method != http::verb::post) return error(405, "Use POST");
    shutdown();
    return response(202, {{"status", "stopping"}});
  }
  if (path == "/v1/jobs") {
    if (method == http::verb::get)
      return response(200, {{"jobs", jobs.list()}});
    return method == http::verb::post ? submit(request, "")
                                      : error(405, "Use GET or POST");
  }
  if (path.starts_with("/v1/commands/"))
    return method == http::verb::post ? submit(request, path.substr(13))
                                      : error(405, "Use POST");
  if (path.starts_with("/v1/jobs/")) {
    const auto id = path.substr(9);
    const auto job = jobs.get(id);
    if (!job) return error(404, "Unknown job");
    if (method == http::verb::get) return response(200, *job);
    if (method == http::verb::delete_) {
      jobs.cancel(id);
      return response(200, *jobs.get(id));
    }
    return error(405, "Use GET or DELETE");
  }
  return error(404, "Unknown endpoint");
}
}
