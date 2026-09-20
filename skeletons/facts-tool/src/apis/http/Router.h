#pragma once
#include "apis/jobs/Queue.h"
#include <boost/beast/http.hpp>
#include <functional>

namespace facts::apis {
using Request = boost::beast::http::request<boost::beast::http::string_body>;
using Response = boost::beast::http::response<boost::beast::http::string_body>;
struct Router {
  Queue &jobs;
  const Settings &settings;
  const std::vector<std::string> &commands;
  std::function<Json()> watchStatus;
  std::function<void()> shutdown;
  Response operator()(const Request &request);
  Response submit(const Request &request, std::string path);
};
Response response(unsigned status, Json body);
Response error(unsigned status, const std::string &message);
}
