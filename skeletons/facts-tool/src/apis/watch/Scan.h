#pragma once
#include "apis/config/Settings.h"
#include "apis/watch/catalog/Catalog.h"
#include <atomic>
#include <expected>
#include <set>

namespace facts::apis::watch {
struct Scan {
  Catalog catalog;
  std::set<std::filesystem::path> roots;
  std::set<std::filesystem::path> directories;
  std::set<std::filesystem::path> databases;
  std::set<std::filesystem::path> controlFiles;
  std::vector<std::string> notices;
};
std::expected<Scan, std::string> discover(const Settings &, Catalog,
                                         const std::atomic_bool &);
}
