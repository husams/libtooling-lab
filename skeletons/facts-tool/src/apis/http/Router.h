#pragma once
#include "apis/jobs/Queue.h"
#include "apis/http/OpenApi.h"
#include "apis/http/Routing.h"
#include <boost/beast/http.hpp>
#include <functional>

namespace facts::apis {
using Request = boost::beast::http::request<boost::beast::http::string_body>;
using Response = boost::beast::http::response<boost::beast::http::string_body>;
struct Router {
  Queue &jobs;
  const Settings &settings;
  const std::vector<std::string> &commands;
  const OpenApiDocuments &documents;
  std::function<Json()> watchStatus;
  std::function<void()> shutdown;
  Response operator()(const Request &request);
  Response submit(const Request &request, std::string path);
  Response dispatch(const Request &request, const MatchedRoute &route);
};
Response response(unsigned status, Json body);
Response textResponse(unsigned status, std::string body, std::string_view contentType);
Response error(unsigned status, const std::string &message);
}
