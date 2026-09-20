#pragma once
#include "apis/jobs/Queue.h"
#include "apis/http/OpenApi.h"
#include "apis/http/Routing.h"
#include "apis/logging/Logger.h"
#include "apis/domain/Selection.h"
#include <boost/beast/http.hpp>
#include <functional>

namespace facts::apis {
namespace runtime { class Service; }
using Request = boost::beast::http::request<boost::beast::http::string_body>;
using Response = boost::beast::http::response<boost::beast::http::string_body>;
using Reply = std::function<void(Response)>;
struct Router {
  Queue &jobs;
  const Settings &settings;
  const std::vector<std::string> &commands;
  const OpenApiDocuments &documents;
  std::function<Json()> watchStatus;
  std::function<void()> shutdown;
  logging::Logger *logger = nullptr;
  runtime::Service *resources = nullptr;
  std::function<domain::Result<Json>(const Json &, bool)> updateWatch;
  std::function<void()> reconcile;
  Response operator()(const Request &request);
  void handle(const Request &request, Reply reply);
  Response resource(const Request &request, const MatchedRoute &route);
  Response submit(const Request &request, std::string path);
  Response dispatch(const Request &request, const MatchedRoute &route);
};
Response response(unsigned status, Json body);
Response textResponse(unsigned status, std::string body, std::string_view contentType);
Response error(unsigned status, const std::string &message);
}
