#include "apis/http/Routing.h"
#include <algorithm>
#include <stdexcept>

namespace facts::apis {
namespace {
int hexadecimal(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}
bool unreserved(char value) {
  return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
         (value >= '0' && value <= '9') || value == '-' || value == '_' ||
         value == '.' || value == '~';
}
std::expected<std::string, HttpError> decode(std::string_view value, bool command) {
  std::string result;
  for (std::size_t i = 0; i < value.size(); ++i) {
    auto character = value[i];
    if (character == '%') {
      if (i + 2 >= value.size() || hexadecimal(value[i + 1]) < 0 ||
          hexadecimal(value[i + 2]) < 0)
        return std::unexpected(HttpError{400, "Invalid path parameter encoding"});
      character = static_cast<char>(hexadecimal(value[i + 1]) * 16 +
                                    hexadecimal(value[i + 2]));
      i += 2;
    }
    if (!unreserved(character) && !(command && character == '/'))
      return std::unexpected(HttpError{400, "Invalid path parameter character"});
    result += character;
  }
  return result;
}
bool matches(const generated::Route &route, std::string_view path) {
  if (route.parameter.empty()) return path == route.path;
  const auto start = route.path.find('{');
  const auto suffix = route.path.substr(route.path.find('}') + 1);
  return path.starts_with(route.path.substr(0, start)) && path.ends_with(suffix) &&
         path.size() > start + suffix.size();
}
}
std::expected<MatchedRoute, HttpError>
matchRoute(std::string_view method, std::string_view path) {
  bool knownPath = false;
  for (const auto &route : generated::routes) {
    if (!matches(route, path)) continue;
    knownPath = true;
    if (method != route.method) continue;
    if (route.parameter.empty()) return MatchedRoute{route.operation, {}};
    const auto start = route.path.find('{');
    const auto suffixSize = route.path.size() - route.path.find('}') - 1;
    return decode(path.substr(start, path.size() - start - suffixSize),
                  route.parameter == "commandPath")
        .transform([&](auto parameter) {
          return MatchedRoute{route.operation, std::move(parameter)};
        });
  }
  return std::unexpected(HttpError{knownPath ? 405U : 404U,
      knownPath ? "Method not supported for this endpoint" : "Unknown endpoint"});
}
const generated::Route &operationRoute(generated::Operation operation) {
  const auto route = std::ranges::find(generated::routes, operation,
                                      &generated::Route::operation);
  if (route == generated::routes.end()) throw std::logic_error("Unknown operation");
  return *route;
}
std::string endpoint(generated::Operation operation, std::string_view parameter) {
  const auto &route = operationRoute(operation);
  std::string path(route.path);
  if (!route.parameter.empty()) {
    const auto start = path.find('{');
    path.replace(start, path.find('}') - start + 1, parameter);
  }
  return path;
}
}
