#pragma once
#include "config/Configuration.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <map>
#include <optional>
#include <unistd.h>

namespace configuration_test {
namespace fs = std::filesystem;
struct Sandbox {
  fs::path previous = fs::current_path();
  fs::path root;
  std::map<std::string, std::optional<std::string>> environment;
  Sandbox() {
    auto pattern = (fs::temp_directory_path() / "facts-config-unit-XXXXXX").string();
    const auto created = mkdtemp(pattern.data());
    assert(created);
    root = fs::canonical(created);
    for (const auto key : {"HOME", "XDG_CONFIG_HOME", "XDG_DATA_HOME",
                           "FACTS_TOOL_CONFIG", "FACTS_TOOL_CONF"}) {
      const auto value = std::getenv(key);
      environment[key] = value ? std::optional<std::string>(value) : std::nullopt;
      unsetenv(key);
    }
    setenv("HOME", root.c_str(), 1);
    fs::current_path(root);
  }
  ~Sandbox() {
    fs::current_path(previous);
    for (const auto &[key, value] : environment)
      if (value) setenv(key.c_str(), value->c_str(), 1);
      else unsetenv(key.c_str());
    fs::remove_all(root);
  }
  fs::path write(const std::string &name, const std::string &content) {
    auto path = root / name;
    fs::create_directories(path.parent_path());
    std::ofstream(path) << content;
    return path;
  }
};
void schema();
void paths();
void discovery();
void arguments();
void ownership();
void policies();
void placeholders();
void safety();
} // namespace configuration_test
