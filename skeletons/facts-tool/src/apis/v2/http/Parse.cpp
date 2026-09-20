#include "apis/v2/http/Dispatch.h"
#include "apis/runtime/Validation.h"
#include <cctype>
#include <vector>

namespace facts::apis::v2::http {
domain::Result<Route> parse(std::string_view target) {
  Route result;
  auto path = target.substr(0, target.find('?'));
  if (!path.starts_with("/api/v2/"))
    return std::unexpected(domain::Error{404, "not_found", "Unknown API resource"});
  path.remove_prefix(8);
  std::vector<std::string> parts;
  while (!path.empty()) {
    const auto slash = path.find('/');
    auto part = runtime::decodeQuery(path.substr(0, slash));
    if (!part || part->empty() || part->size() > 256)
      return std::unexpected(domain::Error{400, "invalid_path", "Invalid resource identifier"});
    for (const unsigned char c : *part)
      if (!std::isalnum(c) && c != '-' && c != '_')
        return std::unexpected(domain::Error{400, "invalid_path", "Invalid resource identifier"});
    parts.push_back(std::move(*part));
    if (slash == std::string_view::npos) break;
    path.remove_prefix(slash + 1);
    if (path.empty()) return std::unexpected(domain::Error{404, "not_found", "Trailing slash is not supported"});
  }
  if (parts.empty() || parts.size() > 4)
    return std::unexpected(domain::Error{404, "not_found", "Unknown API resource"});
  result.resource = parts.front();
  result.jobs = parts.size() > 1 && parts[1] == "job";
  const auto idOffset = result.jobs ? 2U : 1U;
  if (parts.size() > idOffset) result.id = parts[idOffset];
  if (parts.size() > idOffset + 1) result.child = parts[idOffset + 1];
  if (!result.jobs && parts.size() > 3)
    return std::unexpected(domain::Error{404, "not_found", "Unknown API resource"});
  auto query = target.find('?') == std::string_view::npos ? std::string_view{} :
      target.substr(target.find('?') + 1);
  while (!query.empty()) {
    const auto end = query.find('&');
    const auto pair = query.substr(0, end);
    const auto equal = pair.find('=');
    auto key = runtime::decodeQuery(pair.substr(0, equal));
    auto value = runtime::decodeQuery(equal == std::string_view::npos ? "" : pair.substr(equal + 1));
    if (!key || !value || key->empty() || value->empty() || value->size() > 4096 ||
        result.query.contains(*key))
      return std::unexpected(domain::Error{400, "invalid_query", "Invalid or repeated query parameter"});
    result.query[*key] = *value;
    if (end == std::string_view::npos) break;
    query.remove_prefix(end + 1);
  }
  return result;
}
domain::Result<Json> body(const Request &request) {
  const auto type = request[boost::beast::http::field::content_type];
  if (!request.body().empty() && !type.starts_with("application/json"))
    return std::unexpected(domain::Error{415, "unsupported_media_type", "Use application/json"});
  auto value = request.body().empty() ? Json::object() : Json::parse(request.body(), nullptr, false);
  if (!value.is_object())
    return std::unexpected(domain::Error{400, "invalid_request", "JSON body must be an object"});
  return value;
}
}
