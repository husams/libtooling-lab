#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace facts::catalog {

struct Selector {
  std::optional<std::int64_t> id;
  std::string name;
  std::string path;
};

struct ComponentRegistration {
  std::string name;
  std::string path;
  std::string repository;
  std::string kind;
  std::string version;
  bool noGit = false;
};

} // namespace facts::catalog
