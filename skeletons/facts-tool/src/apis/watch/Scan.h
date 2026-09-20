#pragma once
#include "apis/config/Settings.h"
#include "apis/watch/catalog/Catalog.h"
#include <atomic>
#include <expected>
#include <set>
#include <map>

namespace facts::apis::watch {
struct ScanWarning {
  std::string code;
  std::filesystem::path path;
  std::filesystem::path target;
  std::string message;
  bool operator==(const ScanWarning &) const = default;
};
struct Scan {
  Catalog catalog;
  std::map<std::filesystem::path, std::string> inputs;
  std::string signature;
  std::set<std::filesystem::path> roots;
  std::set<std::filesystem::path> directories;
  std::set<std::filesystem::path> databases;
  std::set<std::filesystem::path> controlFiles;
  std::vector<std::string> notices;
  std::vector<ScanWarning> warnings;
  // Lexical aliases are retained even when a physical directory is walked once.
  std::map<std::filesystem::path, std::filesystem::path> aliases;
};
std::expected<Scan, std::string> discover(const Settings &, Catalog,
                                         const std::atomic_bool &);
}
