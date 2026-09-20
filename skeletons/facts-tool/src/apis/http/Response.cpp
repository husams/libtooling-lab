#include "apis/http/Router.h"

namespace facts::apis {
Response response(unsigned status, Json body) {
  Response result{static_cast<boost::beast::http::status>(status), 11};
  result.set(boost::beast::http::field::content_type, "application/json");
  result.set(boost::beast::http::field::server, "facts-tool");
  result.set(boost::beast::http::field::cache_control, "no-store");
  result.keep_alive(false);
  result.body() = body.dump(-1, ' ', false, Json::error_handler_t::replace);
  result.prepare_payload();
  return result;
}
Response error(unsigned status, const std::string &message) {
  return response(status, {{"error", message}});
}
}
