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

inline std::string searched(const std::vector<std::string> &values) {
  std::string result;
  for (const auto &value : values)
    result += (result.empty() ? "" : ", ") + value;
  return result;
}

} // namespace facts::config::detail
