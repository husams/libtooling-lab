#pragma once
#include "apis/generated/Routes.h"
#include <expected>
#include <string>
#include <string_view>

namespace facts::apis {
struct HttpError { unsigned status; std::string message; };
struct MatchedRoute { generated::Operation operation; std::string parameter; };
std::expected<MatchedRoute, HttpError>
matchRoute(std::string_view method, std::string_view path);
const generated::Route &operationRoute(generated::Operation operation);
std::string endpoint(generated::Operation operation, std::string_view parameter = {});
}
