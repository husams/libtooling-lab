#pragma once

#include "config/Configuration.h"

#include <cstdlib>

namespace facts::config::detail {

inline std::string env(const char *name) {
  const auto *value = std::getenv(name);
  return value ? value : "";
}

inline bool present(const char *name) { return std::getenv(name) != nullptr; }

inline std::filesystem::path cwd() { return std::filesystem::canonical("."); }

inline std::filesystem::path projectRoot(std::filesystem::path path) {
  const auto startingPath = path;
  for (;;) {
    if (std::filesystem::exists(path / ".facts-tool.yaml") ||
        std::filesystem::exists(path / ".git"))
      return path;
    const auto parent = path.parent_path();
    if (parent == path) return startingPath;
    path = parent;
  }
}

inline std::filesystem::path builtInRoot() {
  const auto data = env("XDG_DATA_HOME");
  return (!data.empty() ? std::filesystem::path(data)
                        : std::filesystem::path(env("HOME")) / ".local/share") /
         "facts-tool";
}

// The user-tier file: $XDG_CONFIG_HOME/facts-tool/config.yaml, else
// $HOME/.config/facts-tool/config.yaml. When HOME is unset and XDG is also
// unset there is no user file to look for; the caller records that
// symbolically instead of resolving a path.
inline std::string userConfigCandidate() {
  const auto xdg = env("XDG_CONFIG_HOME"), home = env("HOME");
  if (!xdg.empty()) return xdg + "/facts-tool/config.yaml";
  if (!home.empty()) return home + "/.config/facts-tool/config.yaml";
  return "${HOME}/.config/facts-tool/config.yaml";
}

inline std::filesystem::path projectConfigPath(const std::filesystem::path &root) {
  return root / ".facts-tool.yaml";
}

inline std::string searched(const std::vector<std::string> &values) {
  std::string result;
  for (const auto &value : values)
    result += (result.empty() ? "" : ", ") + value;
  return result;
}

// Attributes one setting's problem to the file that supplied it (or
// "built-in" when none did), plus the ordered tiers already walked.
inline std::string settingError(std::string_view key, const std::string &sourceLabel,
                                const std::string &reason,
                                const std::vector<std::string> &discovery) {
  return std::string(key) + " in " + sourceLabel + ": " + reason +
         "; searched: " + searched(discovery) + "; set --config or FACTS_TOOL_CONFIG";
}

// Wraps a readTier() failure with the offending file and the tiers already
// walked, matching what "config show" and compiler consumers report.
inline std::string keyError(const std::string &reason, const std::filesystem::path &path,
                            const std::vector<std::string> &discovery) {
  const auto split = reason.find(' ');
  const auto key = reason.substr(0, split);
  const bool setting = key == "conf_root" || key == "conf_template" ||
                       key == "facts_template" || key == "extra_args";
  return settingError(setting ? key : "configuration", path.string(),
                      setting ? reason.substr(split + 1) : reason, discovery);
}

} // namespace facts::config::detail
