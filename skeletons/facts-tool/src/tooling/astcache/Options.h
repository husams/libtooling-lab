#pragma once

#include <filesystem>

namespace facts::astcache {

struct Options {
  bool enabled = false;
  std::filesystem::path directory;
  int verbosity = 0;
  std::filesystem::path database;
};

} // namespace facts::astcache
