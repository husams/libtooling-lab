#include "apis/http/OpenApi.h"
#include "apis/http/Routing.h"
#include "apis/generated/Spec.h"
#include <algorithm>
#include <cctype>

namespace facts::apis {
namespace {
std::string commandId(std::string_view command) {
  constexpr std::string_view hexadecimal = "0123456789abcdef";
  std::string id = "command_";
  for (const unsigned char character : command) {
    id += hexadecimal[character >> 4];
    id += hexadecimal[character & 15];
  }
  return id;
}
}
Json openApi(const std::vector<std::string> &commands, bool authenticated) {
  auto document = Json::parse(generated::specification());
  auto &paths = document["paths"];
  const auto &route = operationRoute(generated::Operation::command);
  std::string method(route.method);
  std::ranges::transform(method, method.begin(), [](unsigned char c) { return std::tolower(c); });
  const auto operation = paths.at(std::string(route.path)).at(method);
  for (const auto &command : commands) {
    auto concrete = operation;
    concrete.erase("parameters");
    concrete.erase("x-facts-command-expansion");
    concrete["operationId"] = commandId(command);
    concrete["summary"] = "Run " + command;
    concrete["x-cli-command"] = command;
    paths[endpoint(generated::Operation::command, command)][method] = std::move(concrete);
  }
  if (authenticated)
    document["security"] = Json::array({{{"bearerAuth", Json::array()}}});
  else {
    document.erase("security");
    document["components"].erase("securitySchemes");
  }
  return document;
}
OpenApiDocuments openApiDocuments(const std::vector<std::string> &commands,
                                  bool authenticated) {
  const auto document = openApi(commands, authenticated);
  return {document.dump(2), openApiYaml(document)};
}
}
