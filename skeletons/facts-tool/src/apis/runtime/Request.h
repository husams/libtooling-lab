#pragma once
#include "apis/domain/Selection.h"
#include "apis/index/Index.h"
#include <nlohmann/json.hpp>
#include <string_view>
#include <functional>

namespace facts::apis::runtime {
using Json = nlohmann::json;
struct Request {
  std::string operation;
  domain::FileSelector file;
  Json options;
  Settings settings;
  std::function<void(domain::Error &, const std::string &)> reportFileFailure;
  std::function<bool()> cancelled = [] { return false; };
};
domain::Result<Request> parseRequest(std::string operation, const Json &body);
domain::Result<index::Query> parseQuery(std::string_view target);
domain::Result<Json> execute(const domain::Context &, const Request &);
}
