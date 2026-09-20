#pragma once
#include "apis/domain/Selection.h"
#include <string_view>

namespace facts::apis::v2::catalog {
using Json = nlohmann::json;
struct Response {
  unsigned status = 200;
  Json body = nullptr;
  std::string location;
  bool changed = false;
};
// Called on the service's native worker, serialized with import/extraction.
// resource is one of repositories, components, files or directories.
domain::Result<Response> dispatch(const domain::Context &context,
    std::string_view method, std::string_view resource, std::string_view id,
    const Json &body, const Json &query = Json::object());
}
