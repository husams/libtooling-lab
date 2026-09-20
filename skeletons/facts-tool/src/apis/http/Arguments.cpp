#include "apis/http/Arguments.h"
#include "apis/generated/Limits.h"
#include <algorithm>
#include <sstream>

namespace facts::apis {
namespace {
bool containsOption(const std::vector<std::string> &args,
                    const std::string &name, const std::string &alias) {
  return std::ranges::any_of(args, [&](const auto &arg) {
    return arg == name || arg.starts_with(name + "=") ||
           (!alias.empty() && arg.starts_with(alias));
  });
}
void noninteractive(std::vector<std::string> &args) {
  if (args.front() != "symbol") return;
  for (std::size_t i = 1; i < args.size(); ++i) {
    const auto &arg = args[i];
    if (arg == "--conf" || arg == "-c" || arg == "--config" ||
        arg == "--facts" || arg == "-f") { ++i; continue; }
    if (arg == "--verbose" || arg == "-v") {
      if (i + 1 < args.size() && args[i + 1].size() == 1 &&
          args[i + 1][0] >= '0' && args[i + 1][0] <= '3') ++i;
      continue;
    }
    if (arg.starts_with('-')) continue;
    if (arg == "browser") args[i] = "list";
    return;
  }
}
}
std::expected<std::vector<std::string>, std::string>
arguments(const Json &body, const std::string &path,
          const std::vector<std::string> &commands, const Settings &settings) {
  if (!body.is_object() || !body.contains("arguments") ||
      !body["arguments"].is_array())
    return std::unexpected("Expected {\"arguments\": [string, ...]}");
  if (body.size() != 1)
    return std::unexpected("Only the arguments field is supported");
  std::vector<std::string> result;
  if (!path.empty()) {
    if (std::ranges::find(commands, path) == commands.end())
      return std::unexpected("Unknown command path");
    std::istringstream input(path);
    for (std::string part; std::getline(input, part, '/');)
      result.push_back(std::move(part));
  }
  for (const auto &item : body["arguments"]) {
    if (!item.is_string()) return std::unexpected("Arguments must be strings");
    const auto value = item.get<std::string>();
    if (value.find('\0') != std::string::npos || value.size() > generated::maxArgumentBytes)
      return std::unexpected("Invalid argument length or embedded NUL");
    result.push_back(value);
  }
  if (result.empty() || result.size() > generated::maxArguments)
    return std::unexpected("Expected between 1 and " +
                           std::to_string(generated::maxArguments) + " arguments");
  const auto &first = result.front();
  if (first != "--help" && !std::ranges::any_of(commands, [&](const auto &p) {
        return p == first || p.starts_with(first + "/");
      }))
    return std::unexpected("Unknown command; server recursion is prohibited");
  // A terminal browser's data is exposed as the noninteractive symbol list.
  noninteractive(result);
  for (std::size_t i = 0; i + 1 < settings.defaults.size(); i += 2) {
    const auto &option = settings.defaults[i];
    const auto alias = option == "--conf" ? "-c" : "";
    if (!containsOption(result, option, alias)) {
      const auto separator = std::ranges::find(result, "--");
      result.insert(separator, {option, settings.defaults[i + 1]});
    }
  }
  return result;
}
}
