#pragma once
#include "apis/config/Settings.h"
#include <expected>

namespace facts::apis::watch {
struct Clone {
  std::int64_t repositoryId = 0;
  std::int64_t cloneId = 0;
  std::string repository;
  std::string label;
  std::filesystem::path path;
  bool active = false;
  std::string excluded;
  bool operator==(const Clone &) const = default;
};
struct Catalog {
  std::filesystem::path database;
  std::vector<Clone> clones;
  bool operator==(const Catalog &) const = default;
};
std::expected<Catalog, std::string> readCatalog(const Settings &settings);
}
