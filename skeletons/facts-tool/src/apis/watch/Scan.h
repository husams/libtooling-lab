#pragma once
#include "apis/config/Settings.h"
#include <atomic>
#include <expected>
#include <set>

namespace facts::apis::watch {
struct Scan {
  std::set<std::filesystem::path> directories;
  std::set<std::filesystem::path> databases;
};
std::expected<Scan, std::string> discover(const Settings &settings,
                                         const std::atomic_bool &cancelled);
}
