#pragma once
#include "config/ConfigurationDiscovery.h"

namespace facts::config::detail {
inline std::string fileError(const Resolved &value, const std::string &reason) {
  const auto split = reason.find(' ');
  const auto key = reason.substr(0, split);
  const bool setting = key == "conf_root" || key == "conf_template" ||
                       key == "extra_args";
  return (setting ? key : "configuration") + " in " + value.source + ": " +
         (setting ? reason.substr(split + 1) : reason) + "; searched: " +
         searched(value.discovery) + "; set --config or FACTS_TOOL_CONFIG";
}

inline std::vector<std::string> candidates(const std::filesystem::path &root) {
  const auto xdg = env("XDG_CONFIG_HOME"), home = env("HOME");
  return {(root / ".facts-tool.yaml").string(),
          !xdg.empty() ? xdg + "/facts-tool/config.yaml"
          : !home.empty() ? home + "/.config/facts-tool/config.yaml"
                         : "${HOME}/.config/facts-tool/config.yaml"};
}

inline std::expected<Resolved, std::string>
discover(Resolved value, const std::string &selector, Resolved *partial) {
  if (selector.empty() && !env("XDG_CONFIG_HOME").empty() &&
      !std::filesystem::path(env("XDG_CONFIG_HOME")).is_absolute())
    return std::unexpected("XDG_CONFIG_HOME must be absolute");
  auto paths = candidates(value.projectRoot);
  if (!selector.empty()) paths.insert(paths.begin(), (cwd() / selector).string());
  bool selected = false;
  for (const auto &path : paths) {
    if (path.starts_with("${HOME}")) {
      value.discovery.push_back(path + " [inaccessible: HOME is unset]");
      continue;
    }
    if (selected || (!selector.empty() && path != paths.front())) {
      value.discovery.push_back(path + " [skipped]");
      continue;
    }
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error || exists) {
      selected = true;
      value.source = path;
      value.discovery.push_back(path + (error ? " [inaccessible]" : " [selected]"));
    } else {
      value.discovery.push_back(path + " [absent]");
    }
  }
  if (partial) *partial = value;
  if (!selector.empty() && !selected) {
    value.source = paths.front();
    return std::unexpected(fileError(value, "configuration file not found"));
  }
  if (!selected) return value;
  return readYaml(value.source, value).transform_error(
      [&](const auto &reason) { return fileError(value, reason); });
}
} // namespace facts::config::detail
