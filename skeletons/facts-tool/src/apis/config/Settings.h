#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace facts::apis {
struct Settings {
  std::filesystem::path serverConfig;
  std::filesystem::path executable;
  std::filesystem::path workingDirectory;
  std::string host = "127.0.0.1";
  std::uint16_t port = 0;
  bool explicitPort = false;
  bool daemon = false;
  std::string token;
  std::vector<std::string> defaults;
  std::vector<std::filesystem::path> directories;
  std::vector<std::string> importArguments;
  std::vector<std::string> extractArguments;
  unsigned debounceMs = 500;
  unsigned timeoutSeconds = 3600;
};
}
