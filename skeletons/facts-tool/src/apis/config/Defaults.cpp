#include "apis/config/Defaults.h"
#include <set>

namespace facts::apis {
std::expected<void, std::string>
validateDefaults(const std::vector<std::string> &defaults) {
  if (defaults.size() % 2 != 0)
    return std::unexpected("defaults must contain --config/--conf option and path pairs");
  std::set<std::string> seen;
  for (std::size_t index = 0; index < defaults.size(); index += 2) {
    const auto &option = defaults[index];
    if (option != "--config" && option != "--conf")
      return std::unexpected("defaults only supports canonical --config and --conf options");
    if (!seen.insert(option).second)
      return std::unexpected("duplicate option in defaults: " + option);
    if (defaults[index + 1].empty())
      return std::unexpected("default configuration paths must not be empty");
  }
  return {};
}
}
