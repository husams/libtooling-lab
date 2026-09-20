#include "apis/http/Access.h"

namespace facts::apis {
std::expected<void, HttpError> authorize(const Request &request, const Settings &settings) {
  namespace http = boost::beast::http;
  if (request.find(http::field::origin) != request.end())
    return std::unexpected(HttpError{403, "Browser-origin requests are not supported"});
  if (request.find("Sec-Fetch-Site") != request.end())
    return std::unexpected(HttpError{403, "Browser requests are not supported"});
  if (settings.token.empty()) {
    const auto address = settings.host.find(':') == std::string::npos
        ? settings.host : "[" + settings.host + "]";
    const auto host = request[http::field::host];
    const auto port = ":" + std::to_string(settings.port);
    if (host != address && host != address + port &&
        host != "localhost" && host != "localhost" + port)
      return std::unexpected(HttpError{403, "Host must match the local listener address"});
  }
  if (!settings.token.empty() &&
      request[http::field::authorization] != "Bearer " + settings.token)
    return std::unexpected(HttpError{401, "Bearer authentication required"});
  return {};
}
}
